#include "mesh_io.h"
#include "box.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <sstream>

// Small helper to build error messages
static std::string err(const char *msg, const char *fname) {
    std::string s(msg);
    if(fname) { s += ": "; s += fname; }
    return s;
}

void loadObj(const char *filename, std::vector<Vec3f> &vert, std::vector<int> &face) {
    std::ifstream input(filename);
    if(!input)
        throw err("Could not open file", filename);

    std::vector<Vec3f> vertices;
    std::vector<int> faces;
    std::string line;
    while(std::getline(input, line)) {
        std::istringstream row(line);
        std::string tag;
        if(!(row >> tag) || tag[0] == '#')
            continue;

        if(tag == "v") {
            Pos3f vertex;
            if(!(row >> vertex[0] >> vertex[1] >> vertex[2]))
                throw err("Error parsing vertex line", filename);
            vertices.push_back(vertex);
        } else if(tag == "f") {
            std::vector<int> polygon;
            std::string token;
            while(row >> token) {
                if(!token.empty() && token[0] == '#')
                    break;
                const size_t slash = token.find('/');
                const std::string vertexIndex = token.substr(0, slash);
                char *end = nullptr;
                errno = 0;
                const long rawIndex = std::strtol(vertexIndex.c_str(), &end, 10);
                if(vertexIndex.empty() || end == vertexIndex.c_str() || *end != '\0' ||
                   errno == ERANGE || rawIndex == 0)
                    throw err("Could not parse face", filename);
                const long resolved = rawIndex > 0
                    ? rawIndex - 1
                    : static_cast<long>(vertices.size()) + rawIndex;
                if(resolved < 0 || resolved >= static_cast<long>(vertices.size()))
                    throw err("Face index outside vertex array", filename);
                polygon.push_back(static_cast<int>(resolved));
            }
            if(polygon.size() < 3)
                throw err("Face has fewer than three vertices", filename);
            for(size_t i = 1; i + 1 < polygon.size(); ++i) {
                faces.push_back(polygon[0]);
                faces.push_back(polygon[i]);
                faces.push_back(polygon[i + 1]);
            }
        }
    }
    if(!input.eof())
        throw err("Error reading file", filename);
    vert.swap(vertices);
    face.swap(faces);
}

void saveObj(const char *filename, std::vector<Vec3f> &vert, std::vector<Vec3f> &norm, std::vector<int> &face) {
    FILE *fp = fopen(filename, "w");
    if(!fp) throw err("Could not open file for writing", filename);

    for(size_t i = 0; i < vert.size(); i++) {
        Pos3f &v = vert[i];
        fprintf(fp, "v %f %f %f\n", v[0], v[1], v[2]);

        Vec3f &n = norm[i];
        fprintf(fp, "vn %f %f %f\n", n[0], n[1], n[2]);
    }
    fclose(fp);
}

void savePly(const char *filename, std::vector<Vec3f> &vert, std::vector<Vec3f> &norm, std::vector<int> &face) {
    FILE *fp = fopen(filename, "w");
    if(!fp) throw err("Could not open file for writing", filename);

    fprintf(fp, "ply\nformat ascii 1.0\n");
    fprintf(fp, "element vertex %zu\n", vert.size());
    fprintf(fp, "property float x\nproperty float y\nproperty float z\n");
    fprintf(fp, "property float nx\nproperty float ny\nproperty float nz\n");
    fprintf(fp, "end_header\n");

    for(size_t i = 0; i < vert.size(); i++) {
        Pos3f &v = vert[i];
        Vec3f &n = norm[i];
        fprintf(fp, "%f %f %f %f %f %f\n", v[0], v[1], v[2], n[0], n[1], n[2]);
    }
    fclose(fp);
}

void savePlyBinCloud(const char *filename, std::vector<Vec3f> &vert, std::vector<Vec3f> &norm) {
    FILE *fp = fopen(filename, "wb");
    if(!fp) throw err("Could not open file for writing", filename);

    fprintf(fp, "ply\nformat binary_little_endian 1.0\n");
    fprintf(fp, "element vertex %zu\n", vert.size());
    fprintf(fp, "property float x\nproperty float y\nproperty float z\n");
    fprintf(fp, "property float nx\nproperty float ny\nproperty float nz\n");
    fprintf(fp, "end_header\n");

    for(size_t i = 0; i < vert.size(); i++) {
        Pos3f &v = vert[i];
        Vec3f &n = norm[i];
        fwrite(&v, sizeof(Pos3f), 1, fp);
        fwrite(&n, sizeof(Vec3f), 1, fp);
    }
    fclose(fp);
}

void savePlyBinMesh(const char *filename, std::vector<Vec3f> &vert, std::vector<int> &face) {
    FILE *fp = fopen(filename, "wb");
    if(!fp) throw err("Could not open file for writing", filename);

    fprintf(fp, "ply\nformat binary_little_endian 1.0\n");
    fprintf(fp, "element vertex %zu\n", vert.size());
    fprintf(fp, "property float x\nproperty float y\nproperty float z\n");
    fprintf(fp, "element face %zu\n", face.size()/3);
    fprintf(fp, "property list uchar int vertex_indices\n");
    fprintf(fp, "end_header\n");

    fwrite(&*vert.begin(), sizeof(Pos3f), vert.size(), fp);

    unsigned char n = 3;
    for(size_t i = 0; i < face.size(); i += 3) {
        fwrite(&n, 1, 1, fp);
        fwrite(&(face[i]), sizeof(int), 3, fp);
    }
    fclose(fp);
}

void savePlyBinQuadMesh(const char *filename, std::vector<Vec3f> &vert, std::vector<int> &face) {
	FILE *fp = fopen(filename, "wb");
	if(!fp) throw err("Could not open file for writing", filename);

	fprintf(fp, "ply\nformat binary_little_endian 1.0\n");
	fprintf(fp, "element vertex %zu\n", vert.size());
	fprintf(fp, "property float x\nproperty float y\nproperty float z\n");
	fprintf(fp, "element face %zu\n", face.size()/2);
	fprintf(fp, "property list uchar int vertex_indices\n");
	fprintf(fp, "end_header\n");

	fwrite(&*vert.begin(), sizeof(Pos3f), vert.size(), fp);

	unsigned char n = 3;
	for(size_t i = 0; i < face.size(); i += 4) {
		fwrite(&n, 1, 1, fp);
		fwrite(&(face[i]), sizeof(int), 3, fp);
		int f[3] = { face[i+2], face[i+3], face[i] };

		fwrite(&n, 1, 1, fp);
		fwrite(&(f), sizeof(int), 3, fp);
	}
	fclose(fp);
}

void savePlyAsciiMesh(const char *filename, std::vector<Vec3f> &vert, std::vector<int> &face) {
    FILE *fp = fopen(filename, "w");
    if(!fp) throw err("Could not open file for writing", filename);

    fprintf(fp, "ply\nformat ascii 1.0\n");
    fprintf(fp, "element vertex %zu\n", vert.size());
    fprintf(fp, "property float x\nproperty float y\nproperty float z\n");
    fprintf(fp, "element face %zu\n", face.size()/3);
    fprintf(fp, "property list uchar int vertex_indices\n");
    fprintf(fp, "end_header\n");

    for(size_t i = 0; i < vert.size(); i++) {
        Pos3f &v = vert[i];
        fprintf(fp, "%f %f %f\n", v[0], v[1], v[2]);
    }
    for(size_t i = 0; i < face.size(); i += 3) {
        fprintf(fp, "3 %d %d %d\n", face[i], face[i+1], face[i+2]);
    }
    fclose(fp);
}

void savePlyAsciiQuadMesh(const char *filename, std::vector<Vec3f> &vert, std::vector<int> &face) {
    FILE *fp = fopen(filename, "w");
    if(!fp) throw err("Could not open file for writing", filename);

    fprintf(fp, "ply\nformat ascii 1.0\n");
    fprintf(fp, "element vertex %zu\n", vert.size());
    fprintf(fp, "property float x\nproperty float y\nproperty float z\n");
    fprintf(fp, "element face %zu\n", face.size()/2);
    fprintf(fp, "property list uchar int vertex_indices\n");
    fprintf(fp, "end_header\n");

    for(size_t i = 0; i < vert.size(); i++) {
        Pos3f &v = vert[i];
        fprintf(fp, "%f %f %f\n", v[0], v[1], v[2]);
    }
	for(size_t i = 0; i < face.size(); i += 4) {
		fprintf(fp, "3 %d %d %d\n", face[i], face[i+1], face[i+2]);
		fprintf(fp, "3 %d %d %d\n", face[i+2], face[i+3], face[i]);
	}
    fclose(fp);
}
