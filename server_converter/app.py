from flask import Flask, request, redirect, url_for, render_template, flash, send_from_directory, abort, session
import os
import subprocess
import shutil
from werkzeug.utils import secure_filename
import tempfile
import threading

BASE_DIR = os.path.dirname(__file__)
UPLOAD_FOLDER = os.path.join(BASE_DIR, 'uploads')

app = Flask(__name__)
app.config['UPLOAD_FOLDER'] = UPLOAD_FOLDER
# In a production app you should set a strong secret key and don't commit it to git
app.secret_key = 'change-me-for-production'

# Path to converter executable. Resolution order:
# 1. Environment variable CONVERTER_EXE
# 2. Use the explicit full path inside the web_converter folder
# 3. Fallback to `converter.exe` (expect it to be in PATH)
REPO_ROOT = os.path.abspath(os.path.join(BASE_DIR, os.pardir))
# Explicit default full path (use raw string to avoid escape issues)
DEFAULT_CONVERTER = r"C:\Users\fabio\Documents\devel\web_converter\converter.exe"
CONVERTER_EXE = os.environ.get('CONVERTER_EXE') or DEFAULT_CONVERTER
if not os.path.exists(CONVERTER_EXE):
    # fallback to relying on PATH
    CONVERTER_EXE = 'converter.exe'

os.makedirs(UPLOAD_FOLDER, exist_ok=True)


def _safe_relpath(path, start):
    # return path relative to start, ensure it's inside start
    full = os.path.abspath(path)
    start = os.path.abspath(start)
    rel = os.path.relpath(full, start)
    if rel.startswith('..'):
        raise ValueError('path is outside start')
    return rel


@app.route('/', methods=['GET'])
def index():
    # Always clear session on the main page
    session.clear()
    # Render template without any output parameters
    return render_template('index.html')

@app.route('/results', methods=['GET'])
def show_results():
    # Get results from session
    results = session.get('conversion_results', {})
    # Clear session after retrieving results
    session.clear()
    # Render template with results
    return render_template('index.html',
                         stdout=results.get('stdout', ''),
                         stderr=results.get('stderr', ''),
                         files=results.get('files', []),
                         out_dir=results.get('out_dir', None))

@app.route('/upload', methods=['POST'])
def upload():
    if 'file' not in request.files:
        flash('No file part in the request')
        return redirect(url_for('index'))
    file = request.files['file']
    if file.filename == '':
        flash('No file selected')
        return redirect(url_for('index'))
    filename = secure_filename(file.filename)
    dest = os.path.join(app.config['UPLOAD_FOLDER'], filename)
    file.save(dest)
    # Attempt to run the converter on the uploaded file.
    # Contract (assumptions): converter.exe accepts two arguments: <input> <output>
    # If that is not the case for your converter, set CONVERTER_EXE env var
    # to a wrapper script or modify the command below.
    # Create a unique output directory for this conversion run
    out_dir_name = filename + '_converted'
    out_dir = os.path.join(app.config['UPLOAD_FOLDER'], out_dir_name)
    os.makedirs(out_dir, exist_ok=True)

    stdout_text = ''
    stderr_text = ''
    created_files = []
    try:
        # Copy the uploaded file into the output directory and run the converter there.
        # The converter must be invoked as: converter.exe -conv <filename>
        local_input = os.path.join(out_dir, filename)
        shutil.copy2(dest, local_input)

        completed = subprocess.run([CONVERTER_EXE, '-conv', filename], cwd=out_dir, capture_output=True, text=True)
        stdout_text = completed.stdout
        stderr_text = completed.stderr

        # Only list files if converter returned success (0)
        if completed.returncode == 0:
            # List files created by converter under out_dir
            for root, _, files in os.walk(out_dir):
                for f in files:
                    full = os.path.join(root, f)
                    # Skip the copied uploaded input file so it won't be offered for download
                    try:
                        if os.path.abspath(full) == os.path.abspath(local_input):
                            continue
                    except NameError:
                        # If local_input is not defined for some reason, skip the check
                        pass
                    try:
                        rel = _safe_relpath(full, app.config['UPLOAD_FOLDER'])
                    except ValueError:
                        continue
                    created_files.append(rel.replace('\\', '/'))
            # relative output directory (used for download-all)
            out_dir_rel = os.path.relpath(out_dir, app.config['UPLOAD_FOLDER']).replace('\\', '/')
        else:
            # Remove the output directory if conversion failed
            shutil.rmtree(out_dir, ignore_errors=True)
            out_dir_rel = None
    except FileNotFoundError:
        stderr_text = f'Converter not found. Tried: {CONVERTER_EXE}'
        # Clean up output directory in case of error
        shutil.rmtree(out_dir, ignore_errors=True)
        out_dir_rel = None

    # Store the results in the session
    session['conversion_results'] = {
        'stdout': stdout_text,
        'stderr': stderr_text,
        'files': created_files,
        'out_dir': out_dir_rel
    }
    # Redirect to results page
    return redirect(url_for('show_results'))


@app.route('/download/<path:relpath>')
def download(relpath):
    # Serve files from the uploads directory only. relpath is relative to UPLOAD_FOLDER.
    try:
        # Normalize and prevent path traversal
        full = os.path.abspath(os.path.join(app.config['UPLOAD_FOLDER'], relpath))
        if not full.startswith(os.path.abspath(app.config['UPLOAD_FOLDER'])):
            abort(404)
        directory = os.path.dirname(full)
        filename = os.path.basename(full)
        return send_from_directory(directory, filename, as_attachment=True)
    except Exception:
        abort(404)


@app.route('/download-all/<path:rel_dir>')
def download_all(rel_dir):
    # Create a zip archive of the given relative output directory and serve it.
    try:
        full_dir = os.path.abspath(os.path.join(app.config['UPLOAD_FOLDER'], rel_dir))
        if not full_dir.startswith(os.path.abspath(app.config['UPLOAD_FOLDER'])):
            abort(404)
        if not os.path.isdir(full_dir):
            abort(404)

        # Extract original filename from the directory name (remove _converted suffix)
        original_name = os.path.basename(rel_dir)
        if original_name.endswith('_converted'):
            original_name = original_name[:-10]  # remove '_converted' suffix
        
        # Remove the extension from the original filename
        original_name = os.path.splitext(original_name)[0]
        
        tmpdir = tempfile.mkdtemp(prefix='webconv_')
        base_name = os.path.join(tmpdir, original_name)
        zip_path = shutil.make_archive(base_name, 'zip', root_dir=full_dir)
        zip_name = os.path.basename(zip_path)

        # Clean up the temporary directory after a short delay in a background thread.
        def _cleanup(path, d):
            try:
                import time
                time.sleep(5)
                if os.path.exists(path):
                    os.remove(path)
                if os.path.isdir(d):
                    shutil.rmtree(d, ignore_errors=True)
            except Exception:
                pass

        threading.Thread(target=_cleanup, args=(zip_path, tmpdir), daemon=True).start()

        return send_from_directory(os.path.dirname(zip_path), zip_name, as_attachment=True)
    except Exception:
        abort(404)


if __name__ == '__main__':
    # For local testing only. Use a real WSGI server for production.
    app.run(host='127.0.0.1', port=5000, debug=True)
