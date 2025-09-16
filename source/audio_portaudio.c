/*
 * audio_portaudio.h
 * -----------------
 * Sound and music implementation using PortAudio (http://www.portaudio.com/).
 *
 * Only supports loading of WAV files with PCM data for now.
 *
 * By: Marcus 2021
 */

#include "audio.h"
#include "hash_table.h"
#include "portaudio.h"
#include "pthread.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "math.h"
#include "limits.h"
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_FLOAT_OUTPUT
#include "minimp3_ex.h"

/*#define DEBUG_OUTPUT */

/* Portaudio status. */
#define PORT_AUDIO_UNINITIALIZED 0
#define PORT_AUDIO_OK 1
#define PORT_AUDIO_FAILED 2

/* #define SAMPLE_RATE 44100 */
#define SAMPLE_RATE 22050
#define BUFFER_SIZE 256
/* Max number of sounds playing simultaneously, including music. */
#define MAX_SOUNDS 16

/* Don't init portaudio until the program actually uses a function that requires
   it. This way the dll may be omitted. */
static int sInitialized = 0;
static int sPortAudioStatus = PORT_AUDIO_UNINITIALIZED;
static PaStream *sStream;
static pthread_mutex_t sSoundInstanceLock;

/*
 * PortAudioOk
 * -----------
 * Return 1 if portaudio has been successfully initialized. The first time it's
 * called it tries to initialize portaudio. Sequent calls return the same result
 * as the first one, no retries are made.
 */
static int PortAudioOk();

/*
 * AudioCallback
 * -------------
 * Portaudio audio processing callback.
 */
static int AudioCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *userData );


/*
 * SoundData
 * ---------
 * Sound data, used for music and sound effects.
 */
typedef struct {
    unsigned int numSamples; /* Number of samples. */
    unsigned int len;        /* Always numSamples*2. */
    float volume;            /* Only used for music. */
    float *data;             /* Audio data. */
} SoundData;

/*
 * SoundInstance
 * -------------
 * A playing instance of SoundData.
 */
typedef struct {
    SoundData *soundData;   /* Sound data. */
    unsigned int position;  /* Current offset in the audio data. */
    float leftVolume;       /* For panning. */
    float rightVolume;
    char loop;              /* Only used for music. */
} SoundInstance;

/* List of sound instances. */
static SoundInstance sPlayingSounds[MAX_SOUNDS];

/* Loaded music and sound effects. */
static HashTable *sMusic = 0;
static HashTable *sSounds = 0;

/*
 * LoadWav
 * -------
 * Load wav file and return as SoundData on success, only PCM format supported.
 */
static SoundData *LoadWav(const char *filename);

/*
 * LoadMP3
 * -------
 * Load mp3 file and return as SoundData on success.
 */
static SoundData *LoadMP3(const char *filename);

/*
 * BuildSoundData
 * --------------
 * Build single stream SoundData from left/right audio data and down- or up-
 * sample if needed.
 */
static SoundData *BuildSoundData(float *leftData, float *rightData, unsigned int numSamples, unsigned int sampleRate);

/*
 * FreeSoundData
 * -------------
 * Free sound data.
 */
static void FreeSoundData(void *data);

/*
 * AUD_Init
 * --------
 * Called when program starts.
 */
void AUD_Init() {
    if (pthread_mutex_init(&sSoundInstanceLock, 0) == 0) {
        sInitialized = 1;
    }
    else {
        sInitialized = 0;
        return;
    }

    for (int i = 0; i < MAX_SOUNDS; i++) sPlayingSounds[i].soundData = 0;
    sSounds = HT_Create(8);
    sMusic = HT_Create(8);
}

/*
 * AUD_Close
 * ---------
 * Called when program terminates.
 */
void AUD_Close() {
    if (sInitialized) {
        if (sPortAudioStatus == PORT_AUDIO_OK) {
            Pa_AbortStream(sStream);
            Pa_CloseStream(sStream);
            Pa_Terminate();
            sPortAudioStatus = PORT_AUDIO_UNINITIALIZED;
        }
        
        pthread_mutex_destroy(&sSoundInstanceLock);
        
        HT_Free(sSounds, FreeSoundData);
        HT_Free(sMusic, FreeSoundData);
    }
    sInitialized = 0;
    sPortAudioStatus = PORT_AUDIO_UNINITIALIZED;
}

/*
 * AUD_LoadSound
 * -------------
 * Load sound, return 1 on success.
 */
int AUD_LoadSound(int id, const char *filename) {
    if (!sInitialized) return 0;
    
    if (PortAudioOk()) {
        SoundData *soundData;        
        AUD_FreeSound(id);
        if ((soundData = LoadWav(filename))) {
            HT_Add(sSounds, 0, id, soundData);
            return 1;
        }
        else if ((soundData = LoadMP3(filename))) {
            HT_Add(sSounds, 0, id, soundData);
            return 1;
        }
    }

    return 0;
}

/*
 * AUD_CreateSound
 * ---------------
*/
int AUD_CreateSound(int id, float *ldata, float *rdata, int numSamples, int sampleRate) {
    if (sInitialized && PortAudioOk()) {
        SoundData *soundData;
        AUD_FreeSound(id);
        if ((soundData = BuildSoundData(ldata, rdata, numSamples, sampleRate))) {
            HT_Add(sSounds, 0, id, soundData);
            return 1;
        }
    }
    
    return 0;
}


/*
 * AUD_FreeSound
 * -------------
 * Free sound.
 */
void AUD_FreeSound(int id) {
    SoundData *soundData;
    
    if (!sInitialized) return;

    if ((soundData = (SoundData *)HT_Get(sSounds, 0, id))) {
        pthread_mutex_lock(&sSoundInstanceLock);
        for (int i = 0; i < MAX_SOUNDS; i++) {
            if (sPlayingSounds[i].soundData == soundData) sPlayingSounds[i].soundData = 0;
        }
        pthread_mutex_unlock(&sSoundInstanceLock);
        HT_Delete(sSounds, 0, id, FreeSoundData);
    }
}

/*
 * AUD_SoundExists
 * ---------------
 * Return 1 if sound exists.
 */
int AUD_SoundExists(int id) {
    if (!sInitialized) return 0;
    
    return HT_Exists(sSounds, 0, id);
}

/*
 * AUD_PlaySound
 * -------------
 * Play sound with volume vol, [0..1] and panning pan, [-1..1] (-1 = left,
 * 1 = right).
 */
void AUD_PlaySound(int id, float vol, float pan) {
    SoundData *soundData;
    
    if (vol < 0.0f) return;
    
    if (!sInitialized) return;
    
    if (PortAudioOk()) {
        if ((soundData = (SoundData *)HT_Get(sSounds, 0, id))) {
            if (pan < -1.0f) pan = -1.0f;
            if (pan > 1.0f) pan = 1.0f;
            pan = (pan + 1.0f)*0.5f;
            pthread_mutex_lock(&sSoundInstanceLock);
            for (int i = 0; i < MAX_SOUNDS; i++) {
                if (!sPlayingSounds[i].soundData) {
                    sPlayingSounds[i].soundData = soundData;
                    sPlayingSounds[i].position = 0;
                    sPlayingSounds[i].leftVolume = vol*cosf(pan*M_PI*0.5f);
                    sPlayingSounds[i].rightVolume = vol*sinf(pan*M_PI*0.5f);
                    sPlayingSounds[i].loop = 0;
                    break;
                }
            }
            pthread_mutex_unlock(&sSoundInstanceLock);
        }
    }
}

/*
 * AUD_LoadMusic
 * -------------
 * Load music, return 1 on success.
 */
int AUD_LoadMusic(int id, const char *filename) {
    SoundData *soundData;
    
    if (!sInitialized) return 0;
    
    if (PortAudioOk()) {
        AUD_FreeMusic(id);
        if ((soundData = LoadWav(filename))) {
            HT_Add(sMusic, 0, id, soundData);
            return 1;
        }
        else if ((soundData = LoadMP3(filename))) {
            HT_Add(sMusic, 0, id, soundData);
            return 1;
        }
    }
    return 1;
}

/*
 * AUD_FreeMusic
 * -------------
 * Free music.
 */
void AUD_FreeMusic(int id) {
    SoundData *soundData;
    
    if (!sInitialized) return;

    if ((soundData = (SoundData *)HT_Get(sMusic, 0, id))) {
        pthread_mutex_lock(&sSoundInstanceLock);
        for (int i = 0; i < MAX_SOUNDS; i++) {
            if (sPlayingSounds[i].soundData == soundData) sPlayingSounds[i].soundData = 0;
        }
        pthread_mutex_unlock(&sSoundInstanceLock);
        HT_Delete(sMusic, 0, id, FreeSoundData);
    }
}

/*
 * AUD_MusicExists
 * ---------------
 * Return 1 if music exists.
 */
int AUD_MusicExists(int id) {
    if (!sInitialized) return 0;
    
    return HT_Exists(sMusic, 0, id);
}

/*
 * AUD_PlayMusic
 * -------------
 * Play music.
 */
void AUD_PlayMusic(int id, int loop) {
    SoundData *soundData;
    
    if (!sInitialized) return;
    
    if (PortAudioOk()) {
        if ((soundData = (SoundData *)HT_Get(sMusic, 0, id))) {
            pthread_mutex_lock(&sSoundInstanceLock);
            int i;
            /* Is it already playing? */
            for (i = 0; i < MAX_SOUNDS; i++) if (sPlayingSounds[i].soundData == soundData) break;
            if (i < MAX_SOUNDS) {
                sPlayingSounds[i].position = 0;
                sPlayingSounds[i].loop = loop;
            }
            else {
                for (i = 0; i < MAX_SOUNDS; i++) {
                    if (!sPlayingSounds[i].soundData) {
                        sPlayingSounds[i].soundData = soundData;
                        sPlayingSounds[i].position = 0;
                        sPlayingSounds[i].leftVolume = 1.0f;
                        sPlayingSounds[i].rightVolume = 1.0f;
                        sPlayingSounds[i].loop = loop;
                        break;
                    }
                }
            }
            pthread_mutex_unlock(&sSoundInstanceLock);
        }
    }
}

/*
 * AUD_StopMusic
 * -------------
 * Stop music.
 */
void AUD_StopMusic(int id) {
    if (!sInitialized) return;
    
    if (PortAudioOk()) {
        SoundData *soundData;
        if ((soundData = (SoundData *)HT_Get(sMusic, 0, id))) {
            pthread_mutex_lock(&sSoundInstanceLock);
            for (int i = 0; i < MAX_SOUNDS; i++) {
                if (sPlayingSounds[i].soundData == soundData) {
                    sPlayingSounds[i].soundData = 0;
                    break;
                }
            }
            pthread_mutex_unlock(&sSoundInstanceLock);
        }
    }
}

/*
 * AUD_SetMusicVolume
 * ------------------
 * Set music volume, [0..1].
 */
void AUD_SetMusicVolume(int id, float volume) {
    if (!sInitialized) return;
    
    if (PortAudioOk()) {
        SoundData *soundData;
        if ((soundData = (SoundData *)HT_Get(sMusic, 0, id))) {
            pthread_mutex_lock(&sSoundInstanceLock);
            soundData->volume = volume < 0.0f ? 0.0f : volume;
            pthread_mutex_unlock(&sSoundInstanceLock);
        }
    }
}

/*
 * PortAudioOk
 * -----------
 * Return 1 if portaudio has been successfully initialized. The first time it's
 * called it tries to initialize portaudio. Sequent calls return the same result
 * as the first one, no retries are made.
 */
int PortAudioOk() {
    if (sPortAudioStatus == PORT_AUDIO_OK) return 1;
    else if (sPortAudioStatus == PORT_AUDIO_FAILED) return 0;
    
    if (Pa_Initialize() == paNoError) {
        if (Pa_OpenDefaultStream(&sStream, 0, 2, paFloat32, SAMPLE_RATE, BUFFER_SIZE, AudioCallback, 0) == paNoError) {
            if (Pa_StartStream(sStream) == paNoError) {
                sPortAudioStatus = PORT_AUDIO_OK;        
                return 1;
            }
            else {
                Pa_CloseStream(sStream);
                Pa_Terminate();
            }
        }
        else {
            Pa_Terminate();
        }
    }
    
    sPortAudioStatus = PORT_AUDIO_FAILED;
    return 0;
}

/*
 * AudioCallback
 * -------------
 * Portaudio audio processing callback.
 */
int AudioCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *userData) {
    float *out = (float*)outputBuffer;
    
    /* Don't use mutexes, the docs said ... Silly me. */
    pthread_mutex_lock(&sSoundInstanceLock);

    for (unsigned long i = 0; i < framesPerBuffer; i++) {
        float left = 0.0f;
        float right = 0.0f;
        for (int j = 0; j < MAX_SOUNDS; j++) {
            if (sPlayingSounds[j].soundData) {
                left += sPlayingSounds[j].soundData->volume*sPlayingSounds[j].leftVolume*sPlayingSounds[j].soundData->data[sPlayingSounds[j].position++];
                right += sPlayingSounds[j].soundData->volume*sPlayingSounds[j].rightVolume*sPlayingSounds[j].soundData->data[sPlayingSounds[j].position++];
                if (sPlayingSounds[j].position >= sPlayingSounds[j].soundData->len) {
                    if (sPlayingSounds[j].loop) sPlayingSounds[j].position = 0;
                    else sPlayingSounds[j].soundData = 0;
                }
            }
        }
        if (left > 1.0f) left = 1.0f;
        else if (left < -1.0f) left = -1.0f;
        if (right > 1.0f) right = 1.0f;
        else if (right < -1.0f) right = -1.0f;
        *out++ = left;
        *out++ = right;
    }
    
    pthread_mutex_unlock(&sSoundInstanceLock);
    
    return 0;
}

int ReadBE16(FILE *file, unsigned int *out) {
    unsigned char buffer[2];
    if (fread(buffer, sizeof(unsigned char), 2, file) < 2) return 0;
    if (out) *out = buffer[0] | (buffer[1] << 8);
    return 1;
}

int ReadBE16s(FILE *file, int *out) {
    char buffer[2];
    if (fread(buffer, sizeof(char), 2, file) < 2) return 0;
    if (out) *out = (buffer[0] & 0x00ff) | (buffer[1] << 8);
    return 1;
}

int ReadBE32(FILE *file, unsigned int *out) {
    unsigned char buffer[4];
    if (fread(buffer, sizeof(unsigned char), 4, file) < 4) return 0;
    if (out) *out = buffer[0] | (buffer[1] << 8) | (buffer[2] << 16) | (buffer[3] << 24);
    return 1;
}

int ReadBE32s(FILE *file, int *out) {
    char buffer[4];
    if (fread(buffer, sizeof(char), 4, file) < 4) return 0;
    if (out) *out = (buffer[0] & 0x00ff) | ((buffer[1] & 0x00ff) << 8) | ((buffer[2] & 0x00ff) << 16) | (buffer[3] << 24);
    return 1;
}

int ReadBE24s(FILE *file, int *out) {
    char buffer[3];
    if (fread(buffer, sizeof(char), 3, file) < 3) return 0;
    if (out) *out = (buffer[0] & 0x00ff) | ((buffer[1] & 0x00ff) << 8) | (buffer[2] << 16);
    return 1;
}

/*
 * LoadWav
 * -------
 * Load wav file and return as SoundData on success, only PCM format supported.
 */
SoundData *LoadWav(const char *filename) {
    FILE *file = fopen(filename, "rb");
    SoundData *soundData = 0;
    const char *error = 0;
    
#ifdef DEBUG_OUTPUT
    printf("LoadWav %s\n", filename);
#endif
    
    if (file) {
        unsigned char tag[4];
        unsigned int fileSize;
        unsigned int fmtLen;
        unsigned int fmtType;
        unsigned int channels;
        unsigned int sampleRate;
        unsigned int byteRate;
        unsigned int sampleSize;
        unsigned int bitsPerSample;
        unsigned int dataSize;
        unsigned int numSamples;
        float *leftData = 0;
        float *rightData = 0;
        
        fread(tag, 1, 4, file);
        if (strncmp((char *)tag, "RIFF", 4) == 0) {
            if (!(ReadBE32(file, &fileSize) && fread(tag, 1, 4, file) == 4)) goto loadwav_parse_error;

            if (strncmp((char *)tag, "WAVE", 4) == 0) {
                if (fread(tag, 1, 4, file) == 4 && strncmp((char *)tag, "fmt", 3) == 0) {
                    if (ReadBE32(file, &fmtLen) && fmtLen >= 16 && ReadBE16(file, &fmtType) && fmtType == 1) {
                        float divider;
                        if (!(ReadBE16(file, &channels) && ReadBE32(file, &sampleRate) &&
                                ReadBE32(file, &byteRate) && ReadBE16(file, &sampleSize) && 
                                ReadBE16(file, &bitsPerSample))) goto loadwav_parse_error;

                        if ((channels == 1 || channels == 2) &&
                                (bitsPerSample == 8 || bitsPerSample == 16 || bitsPerSample == 24 || bitsPerSample == 32)) {
                            if (bitsPerSample == 8) divider = 127.5f;
                            else if (bitsPerSample == 16) divider = 32767.5f;
                            else if (bitsPerSample == 24) divider = 8388607.5f;
                            else divider = 2147483647.5f;
#ifdef DEBUG_OUTPUT     
                            printf("  pcm, channels: %u, sample rate: %u, byte rate: %u, sample size: %u, bits per sample: %u\n", channels, sampleRate, byteRate, sampleSize, bitsPerSample);
#endif

                            if (fmtLen > 16) fseek(file, fmtLen - 16, SEEK_CUR);
                        
                            /* Skip silly shunks, I hope they all begin with their sizes ... */
                            if (fread(tag, 1, 4, file) < 4) goto loadwav_parse_error;
                            while (strncmp((char *)tag, "data", 4)) {
#ifdef DEBUG_OUTPUT
                                printf("  ignoring chunk: %c%c%c%c\n", tag[0], tag[1], tag[2], tag[3]);
#endif
                                unsigned int chunkSize;
                                if (!(ReadBE32(file, &chunkSize) && 
                                    fseek(file, chunkSize, SEEK_CUR) == 0 && 
                                    fread(tag, 1, 4, file) == 4)) goto loadwav_parse_error;
                            }
                           
                            if (!ReadBE32(file, &dataSize)) goto loadwav_parse_error;
                            numSamples = dataSize/sampleSize;
                            leftData = (float *)malloc(sizeof(float)*numSamples);
                            rightData = (float *)malloc(sizeof(float)*numSamples);

                            /* Mono. */
                            if (channels == 1) {
                                if (bitsPerSample == 8) {
                                    for (unsigned int i = 0; i < numSamples; i++) {
                                        unsigned char tmp;
                                        if (fread(&tmp, 1, 1, file) < 1) goto loadwav_parse_error;
                                        leftData[i] = ((float)tmp - 127.5)/divider;
                                        rightData[i] = ((float)tmp - 127.5)/divider;
                                    }
                                }
                                else if (bitsPerSample == 16) {
                                    for (unsigned int i = 0; i < numSamples; i++) {
                                        int tmp;
                                        if (!ReadBE16s(file, &tmp)) goto loadwav_parse_error;
                                        leftData[i] = (float)tmp/divider;
                                        rightData[i] = (float)tmp/divider;                            
                                    }
                                }
                                else if (bitsPerSample == 24) {
                                    for (unsigned int i = 0; i < numSamples; i++) {
                                        int tmp;
                                        if (!ReadBE24s(file, &tmp)) goto loadwav_parse_error;
                                        leftData[i] = (float)tmp/divider;
                                        rightData[i] = (float)tmp/divider;                            
                                    }
                                }
                                else {
                                    for (unsigned int i = 0; i < numSamples; i++) {
                                        int tmp;
                                        if (!ReadBE32s(file, &tmp)) goto loadwav_parse_error;
                                        leftData[i] = (float)tmp/divider;
                                        rightData[i] = (float)tmp/divider;                
                                    }
                                }                                        
                            }
                            /* Stereo. */
                            else {
                                if (bitsPerSample == 8) {
                                    for (unsigned int i = 0; i < numSamples; i++) {
                                        unsigned char tmp;
                                        if (fread(&tmp, 1, 1, file) < 1) goto loadwav_parse_error;
                                        leftData[i] = ((float)tmp - 127.5)/divider;
                                        if (fread(&tmp, 1, 1, file) < 1) goto loadwav_parse_error;
                                        rightData[i] = ((float)tmp - 127.5)/divider;
                                    }
                                }
                                else if (bitsPerSample == 16) {
                                    for (unsigned int i = 0; i < numSamples; i++) {
                                        int tmp;
                                        if (!ReadBE16s(file, &tmp)) goto loadwav_parse_error;
                                        leftData[i] = (float)tmp/divider;
                                        if (!ReadBE16s(file, &tmp)) goto loadwav_parse_error;
                                        rightData[i] = (float)tmp/divider;                                                                                
                                    }
                                }
                                else if (bitsPerSample == 24) {
                                    for (unsigned int i = 0; i < numSamples; i++) {
                                        int tmp;
                                        if (!ReadBE24s(file, &tmp)) goto loadwav_parse_error;
                                        leftData[i] = (float)tmp/divider;
                                        if (!ReadBE24s(file, &tmp)) goto loadwav_parse_error;
                                        rightData[i] = (float)tmp/divider;                                                                                
                                    }
                                }
                                else {
                                    for (unsigned int i = 0; i < numSamples; i++) {
                                        int tmp;
                                        if (!ReadBE32s(file, &tmp)) goto loadwav_parse_error;
                                        leftData[i] = (float)tmp/divider;
                                        if (!ReadBE32s(file, &tmp)) goto loadwav_parse_error;
                                        rightData[i] = (float)tmp/divider;
                                    }
                                }
                            }
                            soundData = BuildSoundData(leftData, rightData, numSamples, sampleRate);
                        }
                    }
                    else {
                        error = "unsupported format";
                    }                    
                }
                else {
                    error = "missing fmt tag";
                }
            }
            else {
                error = "missing WAVE tag";
            }
        }
        else {
            error = "missing RIFF tag";
        }
        
        goto loadwav_done;
        
loadwav_parse_error:
        error = "parsing failed";
#ifdef DEBUG_OUTPUT
#endif

loadwav_done:
        free(leftData);
        free(rightData);
        fclose(file);
    }
    else {
        error = "file not found";
    }

#ifdef DEBUG_OUTPUT
    if (error) printf("  error: %s\n", error);
#endif
    
    return soundData;
}

/*
 * LoadMP3
 * -------
 */
SoundData *LoadMP3(const char *filename) {
    SoundData *soundData = 0;
    mp3dec_t mp3d;
    mp3dec_file_info_t info;
    int res;

    memset(&info, 0, sizeof(info));    
    res = mp3dec_load(&mp3d, filename, &info, 0, 0);
    
    if (!mp3dec_load(&mp3d, filename, &info, 0, 0)) {
        if (info.channels == 1) {
            soundData = BuildSoundData(info.buffer, info.buffer, info.samples, info.hz);
        }
        else if (info.channels == 2) {
            float *leftData = (float *)malloc(sizeof(float)*info.samples/2);
            float *rightData = (float *)malloc(sizeof(float)*info.samples/2);
            size_t s = info.samples/2;
            for (size_t i = 0; i < s; i++) {
                leftData[i] = info.buffer[i*2];
                rightData[i] = info.buffer[i*2 + 1];
            }
            soundData = BuildSoundData(leftData, rightData, (unsigned int)s, info.hz);
            free(leftData);
            free(rightData);
        }
        free(info.buffer);
    }
    
    return soundData;
}


/*
 * BuildSoundData
 * --------------
 * Build single stream SoundData from left/right audio data and down- or up-
 * sample if needed.
 */
SoundData *BuildSoundData(float *leftData, float *rightData, unsigned int numSamples, unsigned int sampleRate) {
    SoundData *soundData = (SoundData *)malloc(sizeof(SoundData));
    
    if (sampleRate == SAMPLE_RATE) {
        soundData->numSamples = numSamples;
        soundData->len = numSamples*2;
        soundData->data = (float *)malloc(sizeof(float)*soundData->len);
        for (unsigned int i = 0; i < numSamples; i++) {
            soundData->data[i*2] = leftData[i];
            soundData->data[i*2 + 1] = rightData[i];
        }
    }
    else {
        float aspect = (float)sampleRate/(float)SAMPLE_RATE;
        unsigned int resultNumSamples = (unsigned int)((float)numSamples/aspect);
        soundData->numSamples = resultNumSamples;
        soundData->len = resultNumSamples*2;
        soundData->data = (float *)malloc(sizeof(float)*soundData->len);
        for (unsigned int i = 0; i < resultNumSamples; i++) {
            unsigned int srcIndex = (unsigned int)((float)i*aspect);
            float param = (float)i*aspect - (float)srcIndex;
            if (srcIndex < numSamples) {
                unsigned int srcHiIndex = srcIndex + 1;
                if (srcHiIndex >= numSamples) srcHiIndex = srcIndex;
                soundData->data[i*2] = (1.0f - param)*leftData[srcIndex] + param*leftData[srcHiIndex];
                soundData->data[i*2 + 1] = (1.0f - param)*rightData[srcIndex] + param*rightData[srcHiIndex];
            }
            else {
                soundData->data[i*2] = 0;
                soundData->data[i*2 + 1] = 0;
            }

            /*unsigned int srcIndex = (unsigned int)((float)i*aspect);
            if (srcIndex < numSamples) {
                soundData->data[i*2] = leftData[srcIndex];
                soundData->data[i*2 + 1] = rightData[srcIndex];
            }
            else {
                soundData->data[i*2] = 0;
                soundData->data[i*2 + 1] = 0;
            }*/
        }
#ifdef DEBUG_OUTPUT
        printf("BuildSoundData: samples scaled %u -> %u\n", numSamples, resultNumSamples);
#endif
    }
    soundData->volume = 1.0f;
    
    return soundData;
}

/*
 * FreeSoundData
 * -------------
 * Free sound data.
 */
void FreeSoundData(void *data) {
    SoundData *soundData = (SoundData *)data;
    free(soundData->data);
    free(soundData);
}
