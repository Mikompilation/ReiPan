/* ==========================================================================
 *  graphics/movie/story_movie.h
 *
 *  The movie phases, FF3 0x2188f8..0x218d90 (FF3-only file; FF2 had them in
 *  graphics/scene/IngameScene.c). Phase 67 Story_Movie loads the scene's
 *  effect data (scene file + 2) and goes on to its child 90 Story_Movie_Main
 *  when the request wants the movie played; 90 waits for the streams to
 *  stop, plays the PSS with its captions and effects (movie_title.c) and
 *  requests MovieReqGetRetPhase when it ends.
 * ======================================================================== */
#ifndef _GRAPHICS_MOVIE_STORY_MOVIE_H
#define _GRAPHICS_MOVIE_STORY_MOVIE_H

// Phase 90 Story_Movie_Main
int  init_Story_Movie_Main(void);       // FF3 0x002188f8
int  end_Story_Movie_Main(void);        // FF3 0x00218930
int  one_Story_Movie_Main(void);        // FF3 0x00218958

// Phase 67 Story_Movie
int  init_Story_Movie(void);            // FF3 0x00218b18
int  end_Story_Movie(void);             // FF3 0x00218c30
int  one_Story_Movie(void);             // FF3 0x00218c88
void pre_Story_Movie(void);             // FF3 0x00218d50
void after_Story_Movie(void);           // FF3 0x00218d58

void *StoryMovieGetEffectData(void);    // FF3 0x00218d60: null when the scene has none

#endif /* _GRAPHICS_MOVIE_STORY_MOVIE_H */
