LIBS  = -lm -lGdi32 -lWinmm -lcomdlg32 -lportaudio -lwininet -lwinmm
CFLAGS = -Wall -Wno-misleading-indentation -Wno-unused-function -Wno-unused-but-set-variable -fno-align-functions -fno-align-loops

all: n7 renv_console renv_win

n7: source/main_compiler.c source/asm.c source/hash_table.c source/n7.c
	gcc -O3 -o $@ $^ $(CFLAGS)

renv_console: source/main_renv_console.c source/hash_table.c source/n7mm.c source/naalaa_font.c source/naalaa_image.c source/audio_portaudio.c source/renv.c source/stb_image.c source/stb_image_write.c source/syscmd.c source/windowing_winapi.c source/w3d.c source/s3d.c
	gcc -O3 -fno-math-errno -fno-trapping-math -fno-signed-zeros -o $@ $^ $(CFLAGS) $(LIBS) source/renv.res

renv_win: source/main_renv_win.c source/hash_table.c source/n7mm.c source/naalaa_font.c source/naalaa_image.c source/audio_portaudio.c source/renv.c source/stb_image.c source/stb_image_write.c source/syscmd.c source/windowing_winapi.c source/w3d.c source/s3d.c
	gcc -O3 -fno-math-errno -fno-trapping-math -fno-signed-zeros -o $@ $^ $(CFLAGS) $(LIBS) -Wl,--subsystem,windows source/renv.res