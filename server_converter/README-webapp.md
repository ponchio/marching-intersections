# Minimal file upload webapp (moved)

This small Flask app exposes a webpage at `/` where you can upload a file. Uploaded files are saved to `web_converter/uploads/`.

Run on Windows PowerShell (from the `devel` folder which contains this project):

```powershell
# create and activate a venv if you want
python -m pip install -r web_converter/requirements-web.txt
# then run
python -m web_converter.app
# or set FLASK_APP and run flask
$env:FLASK_APP = "web_converter.app"
python -m flask run --host=127.0.0.1 --port=5000
```

Open http://127.0.0.1:5000/ in your browser, choose a file and upload. Files are stored in `web_converter/uploads/`.

Notes:
- This is a minimal example intended for local testing only.
- Change `app.secret_key` in `web_converter/app.py` before using in any non-local environment.
