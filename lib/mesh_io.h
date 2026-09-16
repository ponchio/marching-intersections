#pragma once
#include <vector>
#include "vec.h"

// Use plain C strings for filenames to avoid Qt dependency
void loadObj(const char *filename, std::vector<Vec3f> &vert, std::vector<int> &face);
void saveObj(const char *filename, std::vector<Vec3f> &vert, std::vector<Vec3f> &norm, std::vector<int> &face);
void savePly(const char *filename, std::vector<Vec3f> &vert, std::vector<Vec3f> &norm, std::vector<int> &face);
void savePlyBinCloud(const char *filename, std::vector<Vec3f> &vert, std::vector<Vec3f> &norm);
void savePlyBinMesh(const char *filename, std::vector<Vec3f> &vert, std::vector<int> &face);
void savePlyAsciiMesh(const char *filename, std::vector<Vec3f> &vert, std::vector<int> &face);
void savePlyAsciiQuadMesh(const char *filename, std::vector<Vec3f> &vert, std::vector<int> &face);
