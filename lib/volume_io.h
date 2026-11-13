#ifndef VOLUME_IO_H
#define VOLUME_IO_H

#include "volume.h"

#include <string>

namespace mi {

class VolumeIo {
public:
	explicit VolumeIo(const Volume &volume): volume(volume) {}
	explicit VolumeIo(Volume &volume): volume(volume), destination(&volume) {}

	bool saveJSON(const std::string &path) const;
	bool saveText(const std::string &path) const;
	bool saveBin(const std::string &path) const;
	// Loaders return true on success and throw std::runtime_error for I/O or
	// format validation errors.
	bool loadJSON(const std::string &path, Volume &destination) const;
	bool loadText(const std::string &path, Volume &destination) const;
	bool loadBin(const std::string &path, Volume &destination) const;
	bool loadJSON(const std::string &path) const;
	bool loadText(const std::string &path) const;
	bool loadBin(const std::string &path) const;

private:
	const Volume &volume;
	Volume *destination = nullptr;
};

} // namespace mi

#endif // VOLUME_IO_H