#pragma once

#include <iostream>
#include <string>

class Log {
public:
	enum class Level { Silent, Normal, Debug };

	struct Logger {
		Logger() = default;
		template<typename T>
		Logger &operator<<(const T &v) {
			if (Log::level() == Level::Debug) {
				std::cout << v;
			} else if (Log::level() == Level::Normal) {
				std::cout << v;
			}
			return *this;
		}
	};

	static void setLevel(Level l) { s_level = l; }
	static Level level() { return s_level; }

	static Logger debug;

private:
	static Level s_level;
};
