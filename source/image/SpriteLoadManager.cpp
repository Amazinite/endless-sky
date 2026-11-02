/* SpriteLoadManager.cpp
Copyright (c) 2026 by Amazinite

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see <https://www.gnu.org/licenses/>.
*/

#include "SpriteLoadManager.h"

#include "../Drawable.h"
#include "ImageSet.h"
#include "../Preferences.h"
#include "Sprite.h"
#include "SpriteSet.h"
#include "../TaskQueue.h"

#include <atomic>
#include <map>
#include <queue>
#include <set>

using namespace std;

namespace {
	// If true, sprites will be loaded but not uploaded. Used when the game doesn't
	// need to create a game window (e.g. during testing or when in console-only mode)
	bool preventSpriteUpload = false;

	// Tracks the progress of loading the sprites when the game starts.
	std::atomic<bool> queuedAllImages = false;
	std::atomic<int> spritesLoaded = 0;
	std::atomic<int> totalSprites = 0;
	// List of image sets that are waiting to be loaded at game start.
	mutex imageQueueMutex;
	queue<shared_ptr<ImageSet>> imageQueue;

	// The root folders (starting from the images folder) that use deferred loading.
	set<string> deferredFolders;
	// The sprites that use deferred loading.
	map<const Sprite *, shared_ptr<ImageSet>> deferred;
	// Up to 20 landscape images will be preloaded at a time, with the oldest being culled to make room for new ones.
	const int LANDSCAPE_LIMIT = 20;
	map<const Sprite *, int> preloadedLandscapes;
	// Stellar objects, thumbnails, and other assets remain loaded for up to 100 in-game days before they're culled.
	// This prevents us from repeatedly reloading sprites that the player is frequently encountering.
	const int DAY_LIMIT = 100;
	map<const Sprite *, int> loadedStellarObjects;
	map<const Sprite *, int> loadedThumbnails;
	map<const Sprite *, int> loadedAssets;
	// Scenes remain loaded for only one in-game day before they're culled, as they are not commonly requested.
	// Most scenes are only ever used in a single conversation, for example.
	set<const Sprite *> loadedScenes;
	// Missions and events can add new sprites to the player's current area that may need to be loaded.
	// The code that makes these changes may not have access to the TaskQueue in UI, so they
	// instead send a message to the SpriteLoadManager to tell the current Panel to recheck which
	// sprites should be loaded.
	bool recheckThumbnails = false;
	bool recheckStellarObjects = false;

	// Determine whether the given path or name is for a sprite whose loading
	// should be deferred until needed.
	bool IsDeferredFolder(const filesystem::path &path)
	{
		if(path.empty())
			return false;
		filesystem::path pathStart = *path.begin();
		return deferredFolders.contains(pathStart.string());
	}

	// Loads a sprite and queues it for upload to the GPU.
	void UnloadSprite(TaskQueue &queue, const Sprite *sprite)
	{
		// Don't unload sprites if they were never uploaded to begin with.
		if(preventSpriteUpload)
			return;
		// Unloading needs to be queued on the main thread.
		queue.Run({}, [name = sprite->Name()] { SpriteSet::Modify(name)->Unload(); });
	}

	// Functions for queueing the loading of sprites at game start.
	void LoadSpriteQueued(TaskQueue &queue, const shared_ptr<ImageSet> &image);
	// Loads a sprite from the image queue, recursively.
	void LoadSpriteQueued(TaskQueue &queue)
	{
		if(imageQueue.empty())
			return;

		// Start loading the next image in the list.
		// This is done to save memory on startup.
		LoadSpriteQueued(queue, imageQueue.front());
		imageQueue.pop();
	}

	// Loads a sprite from the given image, with progress tracking.
	// Recursively loads the next image in the queue, if any.
	void LoadSpriteQueued(TaskQueue &queue, const shared_ptr<ImageSet> &image)
	{
		// Deferred images are only minimally loaded so that their dimensions are known.
		Sprite *sprite = SpriteSet::Modify(image->Name());
		if(deferred.contains(sprite))
		{
			queue.Run([image, sprite] { image->LoadDimensions(sprite); },
				[&queue]
				{
					++spritesLoaded;
					// Start loading the next image in the queue, if any.
					lock_guard lock(imageQueueMutex);
					LoadSpriteQueued(queue);
				});
		}
		else
		{
			queue.Run([image] { image->Load(); },
				[image, sprite, &queue]
				{
					image->Upload(sprite, !preventSpriteUpload);
					++spritesLoaded;

					// Start loading the next image in the queue, if any.
					lock_guard lock(imageQueueMutex);
					LoadSpriteQueued(queue);
				});
		}
	}
}



void SpriteLoadManager::Init(TaskQueue &queue, map<string, shared_ptr<ImageSet>> images)
{
	// From the name, strip out any frame number, plus the extension.
	for(auto &[name, imageSet] : images)
	{
		// This should never happen, but just in case:
		if(!imageSet)
			continue;

		// Reduce the set of images to those that are valid.
		imageSet->ValidateFrames();
		// Keep track of which images should use deferred loading.
		if(IsDeferredFolder(name))
			deferred[SpriteSet::Get(name)] = imageSet;
		lock_guard lock(imageQueueMutex);
		imageQueue.push(std::move(imageSet));
		++totalSprites;
	}
	queuedAllImages = true;

	// Launch the tasks to actually load the images, making sure not to exceed the amount
	// of tasks the main thread can handle in a single frame to limit peak memory usage.
	{
		lock_guard lock(imageQueueMutex);
		for(int i = 0; i < TaskQueue::MAX_SYNC_TASKS; ++i)
			LoadSpriteQueued(queue);
	}
}



void SpriteLoadManager::PreventSpriteUpload()
{
	preventSpriteUpload = true;
}



void SpriteLoadManager::FindDeferredFolders()
{
	Preferences::DeferredImageLoading defer = Preferences::GetDeferredImageLoading();
	bool aggressive = defer == Preferences::DeferredImageLoading::AGGRESSIVE;
	bool moderate = aggressive || defer == Preferences::DeferredImageLoading::MODERATE;

	// If deferred image loading is at least minimal, then defer:
	// - landscapes, as they consist of hundreds of 720x360 images.
	// - scenes, as they are rarely used, and many are only used once for a single conversation.
	deferredFolders.insert({"land", "scene"});

	// If deferred image loading is set to at least moderate, then also defer:
	// - outfit and ship thumbnails, as they are only seen while landed or in the map sales panels.
	// - stars and planets, as some rarely seen stars and planets take up a considerable amount of VRAM.
	if(moderate)
		deferredFolders.insert({"thumbnail", "outfit", "star", "planet"});

	// If deferred image loading is set to aggressive, then also defer:
	// - weapon icons, hardpoints, and projectiles.
	// - effects, which can be created by ships, weapons, or hazards.
	// - ship sprites.
	// - asteroids.
	// All of these sprites may only be seen in certain areas of the game, so we can keep all alien content unloaded
	// if a player is still only flying around in human space, as an example.
	if(aggressive)
		deferredFolders.insert({"icon", "hardpoint", "projectile", "effect", "ship", "asteroid"});

	// The only sprites that we never defer are in _menu, font, label, map, and ui.
	// These are all UI-related, and past tests to defer their loading didn't result in a good experience. Either
	// we would have to add a pause whenever the player tries to open certain panels to load in the assets, or
	// the assets would noticeably pop in upon opening the panel. Many of these images are constantly being used as
	// well, so deferring their loading to later would only reduce the VRAM footprint once the game is freshly loaded.
}



double SpriteLoadManager::Progress()
{
	double spriteProgress = 0.;
	if(queuedAllImages)
	{
		if(!totalSprites)
			spriteProgress = 1.;
		else
			spriteProgress = static_cast<double>(spritesLoaded) / totalSprites;
	}
	return spriteProgress;
}



void SpriteLoadManager::LoadSprite(TaskQueue &queue, const shared_ptr<ImageSet> &image)
{
	queue.Run([image] { image->Load(); },
		[image] { image->Upload(SpriteSet::Modify(image->Name()), !preventSpriteUpload); });
}



bool SpriteLoadManager::IsDeferred(const Sprite *sprite)
{
	return deferred.contains(sprite);
}



void SpriteLoadManager::LoadDeferred(TaskQueue &queue, const Drawable &drawable)
{
	LoadDeferred(queue, drawable.GetSprite());
}



void SpriteLoadManager::LoadDeferred(TaskQueue &queue, const Sprite *sprite)
{
	if(!sprite)
		return;
	// Make sure this sprite actually is one that uses deferred loading.
	auto dit = deferred.find(sprite);
	if(dit == deferred.end())
		return;

	const string &name = sprite->Name();
	const shared_ptr<ImageSet> &image = dit->second;
	if(name.starts_with("land/"))
		LoadLandscape(queue, sprite, image);
	else if(name.starts_with("thumbnail/") || name.starts_with("outfit/"))
		LoadThumbnail(queue, sprite, image);
	else if(name.starts_with("star/") || name.starts_with("planet/"))
		LoadStellarObject(queue, sprite, image);
	else if(name.starts_with("scene/"))
		LoadScene(queue, sprite, image);
	else
		LoadAsset(queue, sprite, image);
}



void SpriteLoadManager::CullOldImages(TaskQueue &queue)
{
	auto Cull = [&queue](map<const Sprite *, int> &loadedSprites) -> void {
		for(auto it = loadedSprites.begin(); it != loadedSprites.end(); )
		{
			++it->second;
			if(it->second >= DAY_LIMIT)
			{
				UnloadSprite(queue, it->first);
				it = loadedSprites.erase(it);
			}
			else
				++it;
		}
	};

	Cull(loadedStellarObjects);
	Cull(loadedThumbnails);
	Cull(loadedAssets);

	for(const Sprite *sprite : loadedScenes)
		UnloadSprite(queue, sprite);
	loadedScenes.clear();
}



void SpriteLoadManager::SetRecheckThumbnails()
{
	recheckThumbnails = true;
}



bool SpriteLoadManager::RecheckThumbnails()
{

	bool ret = recheckThumbnails;
	recheckThumbnails = false;
	return ret;
}



void SpriteLoadManager::SetRecheckStellarObjects()
{
	recheckStellarObjects = true;
}



bool SpriteLoadManager::RecheckStellarObjects()
{
	bool ret = recheckStellarObjects;
	recheckStellarObjects = false;
	return ret;
}



void SpriteLoadManager::LoadLandscape(TaskQueue &queue, const Sprite *sprite, const shared_ptr<ImageSet> &image)
{
	// If this sprite is one of the currently loaded ones, there is no need to
	// load it again. But, make note of the fact that it is the most recently
	// asked-for sprite.
	map<const Sprite *, int>::iterator pit = preloadedLandscapes.find(sprite);
	if(pit != preloadedLandscapes.end())
	{
		for(pair<const Sprite *const, int> &it : preloadedLandscapes)
			if(it.second < pit->second)
				++it.second;

		pit->second = 0;
		return;
	}

	// This sprite is not currently preloaded. Check to see whether we already
	// have the maximum number of sprites loaded, in which case the oldest one
	// must be unloaded to make room for this one.
	pit = preloadedLandscapes.begin();
	while(pit != preloadedLandscapes.end())
	{
		++pit->second;
		if(pit->second >= LANDSCAPE_LIMIT)
		{
			UnloadSprite(queue, pit->first);
			pit = preloadedLandscapes.erase(pit);
		}
		else
			++pit;
	}

	// Now, load all the files for this sprite.
	preloadedLandscapes[sprite] = 0;
	LoadSprite(queue, image);
}



void SpriteLoadManager::LoadStellarObject(TaskQueue &queue, const Sprite *sprite, const shared_ptr<ImageSet> &image)
{
	map<const Sprite *, int>::iterator it = loadedStellarObjects.find(sprite);
	if(it != loadedStellarObjects.end())
	{
		it->second = 0;
		return;
	}
	loadedStellarObjects[sprite] = 0;
	LoadSprite(queue, image);
}



void SpriteLoadManager::LoadThumbnail(TaskQueue &queue, const Sprite *sprite, const shared_ptr<ImageSet> &image)
{
	map<const Sprite *, int>::iterator it = loadedThumbnails.find(sprite);
	if(it != loadedThumbnails.end())
	{
		it->second = 0;
		return;
	}
	loadedThumbnails[sprite] = 0;
	LoadSprite(queue, image);
}



void SpriteLoadManager::LoadScene(TaskQueue &queue, const Sprite *sprite, const shared_ptr<ImageSet> &image)
{
	if(!loadedScenes.insert(sprite).second)
		return;

	LoadSprite(queue, image);
}



void SpriteLoadManager::LoadAsset(TaskQueue &queue, const Sprite *sprite, const shared_ptr<ImageSet> &image)
{
	map<const Sprite *, int>::iterator it = loadedAssets.find(sprite);
	if(it != loadedAssets.end())
	{
		it->second = 0;
		return;
	}
	loadedAssets[sprite] = 0;
	LoadSprite(queue, image);
}
