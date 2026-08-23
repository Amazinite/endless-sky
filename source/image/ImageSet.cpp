/* ImageSet.cpp
Copyright (c) 2017 by Michael Zahniser

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see <https://www.gnu.org/licenses/>.
*/

#include "ImageSet.h"

#include "../text/Format.h"
#include "../GameData.h"
#include "ImageBuffer.h"
#include "ImageFileData.h"
#include "../Logger.h"
#include "Mask.h"
#include "MaskManager.h"
#include "Sprite.h"

#include <algorithm>
#include <cassert>

using namespace std;

namespace {
	const int PATH_1X_MAIN = 0;
	const int PATH_2X_MAIN = 1;
	const int PATH_1X_MASK = 2;
	const int PATH_2X_MASK = 3;

	const int BUFFER_MAIN = 0;
	const int BUFFER_MASK = 1;

	// Determine whether the given path or name is to a sprite for which a
	// collision mask ought to be generated.
	bool IsMasked(const filesystem::path &path)
	{
		if(path.empty())
			return false;
		filesystem::path directory = *path.begin();
		return directory == "ship" || directory == "asteroid";
	}

	// Add consecutive frames from the given map to the given vector. Issue warnings for missing or mislabeled frames.
	void AddValid(const map<size_t, filesystem::path> &frameData, vector<filesystem::path> &sequence,
		const string &prefix, bool is2x, bool isSwizzleMask) noexcept(false)
	{
		if(frameData.empty())
			return;
		// Valid animations (or stills) begin with frame 0.
		if(frameData.begin()->first != 0)
		{
			Logger::Log(prefix + "ignored " + (isSwizzleMask ? "mask " : "") + (is2x ? "@2x " : "")
				+ "frame " + to_string(frameData.begin()->first) + " (" + to_string(frameData.size())
				+ " ignored in total). Animations must start at frame 0.", Logger::Level::WARNING);
			return;
		}

		// Find the first frame that is not a single increment over the previous frame.
		auto it = frameData.begin();
		auto next = it;
		auto end = frameData.end();
		while(++next != end && next->first == it->first + 1)
			it = next;
		// Copy the sorted, valid paths from the map to the frame sequence vector.
		size_t count = distance(frameData.begin(), next);
		sequence.resize(count);
		transform(frameData.begin(), next, sequence.begin(),
			[](const pair<size_t, filesystem::path> &p) -> filesystem::path { return p.second; });

		// If `next` is not the end, then there was at least one discontinuous frame.
		if(next != frameData.end())
		{
			size_t ignored = distance(next, frameData.end());
			Logger::Log(prefix + "missing " + (isSwizzleMask ? "mask " : "") + (is2x ? "@2x " : "") + "frame "
				+ to_string(it->first + 1) + " (" + to_string(ignored)
				+ (ignored > 1 ? " frames" : " frame") + " ignored in total).", Logger::Level::WARNING);
		}
	}
}



// Check if the given path is to an image of a valid file type.
bool ImageSet::IsImage(const filesystem::path &path)
{
	filesystem::path ext = path.extension();
	return ImageBuffer::ImageExtensions().contains(Format::LowerCase(ext.string()));
}



ImageSet::ImageSet(string name)
	: name(std::move(name))
{
}



// Get the name of the sprite for this image set.
const string &ImageSet::Name() const
{
	return name;
}



// Whether this image set is empty, i.e. has no images.
bool ImageSet::IsEmpty() const
{
	return framePaths[PATH_1X_MAIN].empty() && framePaths[PATH_2X_MAIN].empty();
}



// Add a single image to this set. Assume the name of the image has already
// been checked to make sure it belongs in this set.
void ImageSet::Add(ImageFileData data)
{
	// Determine which frame of the sprite this image will be.
	// Store the requested path.
	framePaths[data.is2x + (2 * data.isSwizzleMask)][data.frameNumber].swap(data.path);
	noReduction |= data.noReduction;
}



// Reduce all given paths to frame images into a sequence of consecutive frames.
void ImageSet::ValidateFrames() noexcept(false)
{
	string prefix = "Sprite \"" + name + "\": ";
	AddValid(framePaths[PATH_1X_MAIN], paths[PATH_1X_MAIN], prefix, false, false);
	AddValid(framePaths[PATH_2X_MAIN], paths[PATH_2X_MAIN], prefix, true, false);
	AddValid(framePaths[PATH_1X_MASK], paths[PATH_1X_MASK], prefix, false, true);
	AddValid(framePaths[PATH_2X_MASK], paths[PATH_2X_MASK], prefix, true, true);
	framePaths[PATH_1X_MAIN].clear();
	framePaths[PATH_2X_MAIN].clear();
	framePaths[PATH_1X_MASK].clear();
	framePaths[PATH_2X_MASK].clear();

	// Ensure that image sequences aren't mixed with other images.
	for(int i = 0; i < 4; ++i)
		for(const auto &path : paths[i])
		{
			string ext = path.extension().string();
			if(ImageBuffer::ImageSequenceExtensions().contains(Format::LowerCase(ext)) && paths[i].size() > 1)
			{
				Logger::Log("Image sequences must be exclusive; ignoring all but the image sequence data for \""
					+ name + "\".", Logger::Level::WARNING);
				paths[i][0] = path;
				paths[i].resize(1);
				break;
			}
		}

	// Determine if we are using 1x or 2x paths for the main and mask buffers.
	// If 2x paths are present, those will be the paths that are used to populate the buffers.
	// Warn if the number of 1x and 2x paths do not match if both sets of paths are present,
	// as this is likely a content creation error.
	if(!paths[PATH_2X_MAIN].empty())
		is2x[BUFFER_MAIN] = true;
	if(is2x[BUFFER_MAIN] && !paths[PATH_1X_MAIN].empty() && paths[PATH_1X_MAIN].size() != paths[PATH_2X_MAIN].size())
		Logger::Log(prefix + "the number of normal resolution frames does not match the number of @2x frames. "
			"Visuals may not match what is expected.", Logger::Level::WARNING);
	if(!paths[PATH_2X_MASK].empty())
		is2x[BUFFER_MASK] = true;
	if(is2x[BUFFER_MASK] && !paths[PATH_1X_MASK].empty() && paths[PATH_1X_MASK].size() != paths[PATH_2X_MASK].size())
		Logger::Log(prefix + "the number of normal resolution mask frames does not match the number of @mask 2x "
			"frames. Collision masks may not match what is expected.", Logger::Level::WARNING);

	// The number of mask frames must either be 0, 1, or equal the number of main frames.
	int mainFrames = paths[is2x[BUFFER_MAIN]].size();
	int maskFrames = paths[2 + is2x[BUFFER_MASK]].size();
	if(maskFrames > 1 && maskFrames != mainFrames)
	{
		string specifier = is2x[BUFFER_MASK] ? "@2x mask" : "mask";
		Logger::Log(prefix + "Discarding " + to_string(maskFrames - 1) + " frames of " + specifier + " because there"
			" are more frames of animation. Only the first swizzle mask frame will be used.", Logger::Level::WARNING);
		paths[2 + is2x[BUFFER_MASK]].resize(1);
	}
}



// Load all the frames. This should be called in one of the image-loading
// worker threads. This also generates collision masks if needed.
void ImageSet::Load() noexcept(false)
{
	assert(framePaths[is2x[BUFFER_MAIN]].empty() && "should call ValidateFrames before calling Load");

	// Determine how many frames there will be, total. The image buffers will
	// not actually be allocated until the first image is loaded (at which point
	// the sprite's dimensions will be known).
	size_t frames = paths[is2x[BUFFER_MAIN]].size();
	size_t swizzleMaskFrames = paths[2 + is2x[BUFFER_MASK]].size();

	// Check whether we need to generate collision masks.
	bool makeMasks = IsMasked(name);

	const auto UpdateFrameCount = [&]()
	{
		buffer[BUFFER_MASK].Clear(swizzleMaskFrames);

		if(makeMasks)
			masks.resize(frames);
	};

	buffer[BUFFER_MAIN].Clear(frames);
	UpdateFrameCount();

	// Load the 1x sprites first, then the 2x sprites, because they are likely
	// to be in separate locations on the disk. Create masks if needed.
	for(size_t i = 0; i < paths[is2x[BUFFER_MAIN]].size(); ++i)
	{
		int loadedFrames = buffer[BUFFER_MAIN].Read(paths[is2x[BUFFER_MAIN]][i], i);
		const string fileName = "\"" + name + "\" frame #" + to_string(i);
		if(!loadedFrames)
		{
			Logger::Log("Failed to read image data for " + fileName, Logger::Level::WARNING);
			continue;
		}
		// If we loaded an image sequence, clear all other buffers.
		if(loadedFrames > 1)
		{
			frames = loadedFrames;
			UpdateFrameCount();
		}

		if(makeMasks)
		{
			masks[i].Create(buffer[BUFFER_MAIN], is2x[BUFFER_MAIN], i, fileName);
			if(!masks[i].IsLoaded())
				Logger::Log("Failed to create collision mask for " + fileName, Logger::Level::WARNING);
		}
	}

	auto LoadSprites = [&](const vector<filesystem::path> &toLoad, ImageBuffer &buffer, const string &specifier)
	{
		for(size_t i = 0; i < frames && i < toLoad.size(); ++i)
			if(!buffer.Read(toLoad[i], i))
			{
				Logger::Log("Removing " + specifier + " frames for \"" + name + "\" due to read error",
					Logger::Level::WARNING);
				buffer.Clear();
				break;
			}
	};

	// Now, load the mask sprites, if they exist.
	LoadSprites(paths[2 + is2x[BUFFER_MASK]], buffer[BUFFER_MASK], is2x[BUFFER_MASK] ? "@2x mask" : "mask");

	// Warn about a "high-profile" image that will be blurry due to rendering at
	// 50% scale for 1x sprites and 25% scale for 2x sprites.
	int modBy = is2x[BUFFER_MAIN] ? 4 : 2;
	string even = is2x[BUFFER_MAIN] ? "divisible by 4" : "even";
	bool willBlur = (buffer[BUFFER_MAIN].Width() % modBy) || (buffer[BUFFER_MAIN].Height() % modBy);
	if(willBlur && (name.starts_with("ship/") || name.starts_with("outfit/") || name.starts_with("thumbnail/")))
		Logger::Log("Image \"" + name + "\" will be blurry since width and/or height are not " + even + " ("
			+ to_string(buffer[BUFFER_MAIN].Width()) + "x" + to_string(buffer[BUFFER_MAIN].Height()) + ").",
			Logger::Level::WARNING);
}



void ImageSet::LoadDimensions(Sprite *sprite) noexcept(false)
{
	assert(framePaths[is2x[BUFFER_MAIN]].empty() && "should call ValidateFrames before calling LoadDimensions");

	// Read only the first frame in order to determine the dimensions of the sprite.
	// (All frames are expected to have the same dimensions.)
	size_t frames = paths[is2x[BUFFER_MAIN]].size();
	if(!frames)
		return;
	buffer[BUFFER_MAIN].Clear(frames);
	int loadedFrames = buffer[BUFFER_MAIN].Read(paths[is2x[BUFFER_MAIN]][0], 0, true);
	if(!loadedFrames)
	{
		Logger::Log("Failed to read image data for \"" + name + "\" frame #0.", Logger::Level::WARNING);
		return;
	}
	sprite->LoadDimensions(buffer[BUFFER_MAIN], is2x[BUFFER_MAIN]);
	// Clear the buffer since no image data was actually uploaded.
	buffer[BUFFER_MAIN].Clear();
}



// Create the sprite and optionally upload the image data to the GPU. After this is
// called, the internal image buffers and mask vector will be cleared, but
// the paths are saved in case the sprite needs to be loaded again.
void ImageSet::Upload(Sprite *sprite, bool enableUpload)
{
	// Clear all the buffers if we are not uploading the image data.
	if(!enableUpload)
		for(ImageBuffer &it : buffer)
			it.Clear();

	// Load the frames (this will clear the buffers).
	sprite->AddFrames(buffer[BUFFER_MAIN], is2x[BUFFER_MAIN], noReduction);
	sprite->AddSwizzleMaskFrames(buffer[BUFFER_MASK], noReduction);

	GameData::GetMaskManager().SetMasks(sprite, std::move(masks));
	masks.clear();
}
