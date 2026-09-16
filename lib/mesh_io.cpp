#include "mesh_io.h"
#include "box.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <cerrno>

// Small helper to build error messages
static std::string err(const char *msg, const char *fname) {
    std::string s(msg);
    if(fname) { s += ": "; s += fname; }
    return s;
}

void loadObj(const char *filename, std::vector<Vec3f> &vert, std::vector<int> &face) {
    char buffer[1024];
    FILE *fp = fopen(filename, "r");
    if(!fp)
        throw err("Could not open file", filename);

    while(fgets(buffer, sizeof(buffer), fp) != NULL) {
        int s = (int)strlen(buffer);
        if(s == 0) continue;
        while(s > 0 && (buffer[s-1] == '\n' || buffer[s-1] == '\r')) { buffer[s-1] = '\0'; --s; }
        if(s == 0) continue;
        if(buffer[0] == '#') continue;

        if(buffer[0] == 'v') {
            if(buffer[1] == ' ') {
                Pos3f v;
                int n = sscanf(buffer, "v %f %f %f", &(v[0]), &(v[1]), &(v[2]));
                if(n != 3) throw err("Error parsing vertex line", buffer);
                vert.push_back(v);
            }
            continue;
        }
        if(buffer[0] == 'f') {
            int f[4];
            int res=sscanf(buffer, "f %d %d %d %d", &f[0], &f[1], &f[2], &f[3]);
            if (res !=4 && res != 3) {
                int dummy;
                res=sscanf(buffer, "f %d//%d %d//%d %d//%d %d//%d", &f[0], &dummy, &f[1], &dummy,  &f[2] , &dummy ,  &f[3] , &dummy);
                if(res != 8 && res != 6) {
                    fclose(fp);
                    throw err("Could not parse face", buffer);
                }
                res /=2;
            }
            if (res == 3) {
                for(int i = 0; i < 3; i++) face.push_back(f[i] -1);
            }
            if (res == 4) {
                for(int i = 0; i < 3; i++) face.push_back(f[i] -1);
                for(int i = 2; i < 5; i++) face.push_back(f[i%4] -1);
            }
        }
    }
    fclose(fp);
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
