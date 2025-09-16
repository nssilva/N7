/*
 * audio.h
 * -------
 * Sound and music.
 *
 * By: Marcus 2021 
 */

/*
 * AUD_Init
 * --------
 * Called when program starts.
 */
void AUD_Init();

/*
 * AUD_Close
 * ---------
 * Called when program terminates.
 */
void AUD_Close();

/*
 * AUD_LoadSound
 * -------------
 * Load sound, return 1 on success.
 */
int AUD_LoadSound(int id, const char *filename);

/*
 * AUD_CreateSound
 * ---------------
*/
int AUD_CreateSound(int id, float *ldata, float *rdata, int numData, int sampleRate);

/*
 * AUD_FreeSound
 * -------------
 * Free sound.
 */
void AUD_FreeSound(int id);

/*
 * AUD_SoundExists
 * ---------------
 * Return 1 if sound exists.
 */
int AUD_SoundExists(int id);

/*
 * AUD_PlaySound
 * -------------
 * Play sound with volume vol, [0..1] and panning pan, [-1..1] (-1 = left,
 * 1 = right).
 */
void AUD_PlaySound(int id, float vol, float pan);

/*
 * AUD_LoadMusic
 * -------------
 * Load music, return 1 on success.
 */
int AUD_LoadMusic(int id, const char *filename);

/*
 * AUD_FreeMusic
 * -------------
 * Free music.
 */
void AUD_FreeMusic(int id);

/*
 * AUD_MusicExists
 * ---------------
 * Return 1 if music exists.
 */
int AUD_MusicExists(int id);

/*
 * AUD_PlayMusic
 * -------------
 * Play music.
 */
void AUD_PlayMusic(int id, int loop);

/*
 * AUD_StopMusic
 * -------------
 * Stop music.
 */
void AUD_StopMusic(int id);

/*
 * AUD_SetMusicVolume
 * ------------------
 * Set music volume, [0..1].
 */
void AUD_SetMusicVolume(int id, float volume);
