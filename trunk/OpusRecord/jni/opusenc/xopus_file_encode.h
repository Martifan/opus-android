typedef long (*audio_read_func)(void *src, short *buffer, int samples);
void opus_file_encode( const char* outFile, int tkrate, int tkchannels, int tkendianness, int tksamplesize, audio_read_func tk_readfunc );
