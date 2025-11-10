#pragma once

#include "Common.h"
#include "PlatformUtils.h"

namespace cz
{

/**
 * Very simple file reading/writing class
 * Just because I hate using STL streams. 
 *
 * WARNING: Some of the C functions used (e.g: ftell) use signed 32 bits values, therefore this class should only be used for
 * files <= 2GB in size
 */
class File
{
  public:

	/**
	 * File opening mode. See https://en.cppreference.com/w/cpp/io/c/fopen
	 */
	enum class Mode
	{
		/**
		 * Open for reading. Translates to C's "r" mode
		 * - Action if file already exists     : Read from start
		 * - Action if the file does not exist : Fails to open
		 */
		Read,

		/**
		 * Open for writing. Translates to C's "w" mode
		 * - Action if file already exists     : Destroy contents
		 * - Action if the file does not exist : Create new
		 */
		Write,

		/**
		 * Opens for appending. Translates to C's "a"
		 * - Action if file already exists     : Write to end
		 * - Action if the file does not exist : Create new
		 */
		Append,

		/**
		 * Opens for read&write and the file needs to exist. Translates to C's "r+"
		 * - Action if file already exists     : Read from start
		 * - Action if the file does not exist : Fails to open
		 */
		ReadWriteExisting,

		/**
		 * Opens for read&write overwriting if the file already exists. Translates to C's "w+"
		 * - Action if file already exists     : Destroy contents
		 * - Action if the file does not exist : Create new
		 */
		ReadWriteNew,

		/**
		 * Opens for read&write and sets the cursor to the end of the file . Translates to C's "a+"
		 * - Action if file already exists     : Write to end
		 * - Action if the file does not exist : Create new
		 */
		ReadWriteAppend
	};

	enum class SeekMode
	{
		Set,
		Current,
		End
	};

	CZ_DELETE_COPY_AND_MOVE(File);

	~File();

	/**
	 * Opens the file
	 * It logs an error if the file can't be opened.
	 *
	 * @param path Path to the file. No attempt is made to resolve relative paths.
	 */
	static std::unique_ptr<File> open(const fs::path& path, Mode mode);

	/**
	 * Opens the file
	 * It doesn't log an error if the file can't be opened
	 *
	 * @param path Path to the file. No attempt is made to resolve relative paths.
	 */
	static std::unique_ptr<File> try_open(const fs::path& path, Mode mode);

	size_t write(const void* buffer, size_t size, size_t count);
	size_t read(void* buffer, size_t size, size_t count);
	size_t write(const void* buffer, size_t bytes);
	size_t read(void* buffer, size_t bytes);
	bool eof() const;
	size_t tell() const;
	bool seek(size_t offset, SeekMode seekMode);

	size_t size();

	const fs::path& getPath() const
	{
		return m_path;
	}

	//! Returns the internal FILE handle.
	/*
	* NOTE: Be careful what you do with it.
	*/
	FILE* getHandle()
	{
		return m_handle;
	}

	/*!
	 * Helper structure to make it easier to load an entire file into a memory buffer.
	 */
	struct Buffer
	{
		CZ_DELETE_COPY(Buffer);

		// Since 0 is a valid size (the file is empty), we use max() to signify there was an error
		inline static constexpr size_t InvalidSize = std::numeric_limits<size_t>::max();

		Buffer(size_t size)
		{
			ptr = reinterpret_cast<char*>(malloc(size ? size : 1));
			// IMPORTANT: `size` is intentionally NOT set here, so the caller code can set it ONLY if there was no error
		}

		Buffer() = default;

		Buffer(Buffer&& other)
		{
			std::swap(ptr, other.ptr);
			std::swap(size, other.size);
		}

		~Buffer()
		{
			free(ptr);
		}

		Buffer& operator=(Buffer&& other)
		{
			free(ptr);
			ptr = other.ptr;
			size = other.size;
			other.ptr = nullptr;
			other.size = InvalidSize;
			return *this;
		}

		bool isValid() const
		{
			return size != InvalidSize;
		}

		operator bool() const
		{
			return isValid();
		}

		std::string_view to_string_view() const
		{
			if (isValid())
				return std::string_view(ptr, size);
			else
				return "";
		}

		char* ptr = nullptr;
		size_t size = InvalidSize;
	};

	/*!
	 * Reads the entire contents of a file into a buffer
	 */
	static Buffer readAll(const fs::path& path, bool logErrors = true)
	{
		return readAllImpl(path, logErrors);
	}

	static Buffer try_readAll(const fs::path& path)
	{
		return readAllImpl(path, false);
	}

protected:

	static Buffer readAllImpl(const fs::path& path, bool raiseError);

	static std::unique_ptr<File> openImpl(const fs::path& path, Mode mode, bool raiseError);
	fs::path m_path;
	FILE* m_handle = nullptr;
	Mode m_mode;

	// Provides a way to use make_shared with private/protected constructors
	struct this_is_private
	{
		explicit this_is_private(int) {}
	};

public:	
	File(const this_is_private) {}

};


/*!
 * Saves content to a text file.
 *
 * \param saveOnlyIfChanged
 *	Instead of saving to the file straight away, it first loads the existing contents, compares them, and only saves
 *	the new data if it changed. This is to avoid changing the file timestamps.
 */
bool saveTextFile(const fs::path& path, std::string_view contents, bool saveOnlyIfChanged);

 /**
  * Given a filename (full path), it checks if the file exists and if it does, it renames it so that the name
  * contains the time the file was last modified.
  *
  * This is useful for when you want to create some log file but wants to keep the old file.
  * The end result, is that for example, all log files are kept.
  */
 bool renameFileToTimestamp(const std::filesystem::path& filename);

} // namespace cz



