#include <windows.h>
#include <stdio.h>

extern "C"
{
	#include "libavcodec/avcodec.h"
	#include "libavformat/avformat.h"
	#include "libswscale/swscale.h"

	#include "libavutil/avutil.h"
	#include "libavutil/avstring.h"
}

#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "swscale.lib")

#pragma comment(lib, "avutil.lib")




char *INPUT_FILE = "D:/Users/phil/Desktop/test.mp4";




#define u8 uint8_t


void MsgBox(char* s) {
    MessageBox(0, s, "vid player", MB_OK);
}

void SaveFrame(AVFrame *frame, int width, int height, int frame_index)
{
	FILE *file;
	char filename[256];
	int y;

	sprintf(filename, "frame%d.ppm", frame_index);
	file=fopen(filename, "wb");
	if (file==0) return;

		// file header
		fprintf(file, "P6\n%d %d\n255\n", width, height);

		// Write pixel data
		for(y = 0; y < height; y++)
		fwrite(frame->data[0]+y*frame->linesize[0], 1, width*3, file);

	fclose(file);
}







int main(int argc, char *argv[])
{

	// register all formats and codecs
	av_register_all();


	AVFormatContext *fc = 0;  // =0 or call avformat_alloc_context?

	int open_result = avformat_open_input(&fc, INPUT_FILE, 0, 0);
	if (open_result != 0)
	{
		MsgBox("Can't open file.");
		char errbuf[1024];
		av_strerror(open_result, errbuf, 1024);
		MsgBox(errbuf);
		return -1;
	}

	// populate fc->streams
	if (avformat_find_stream_info(fc, 0) < 0)
	{
		MsgBox("Can't find stream info.");
		return -1;
	}

	av_dump_format(fc, 0, INPUT_FILE, 0);


	// find first video stream
	int videoStream = -1;
	for (int i = 0; i < fc->nb_streams; i++)
	{
		if (fc->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO)
		{
			videoStream = i;
			break;
		}
	}
	if (videoStream == -1)
	{
		MsgBox("No video stream found.");
		return -1;
	}


	// shortcut to the codec context
	AVCodecContext *cc_orig = fc->streams[videoStream]->codec;


	// find the actual decoder
	AVCodec *codec = avcodec_find_decoder(cc_orig->codec_id);
	if (!codec)
	{
		MsgBox("Unsupported codec. Yipes.");
		return -1;
	}

	// copy the codec context (don't use directly from cc_orig)
	AVCodecContext *cc = avcodec_alloc_context3(codec);
	if (avcodec_copy_context(cc, cc_orig) != 0)
	{
		MsgBox("Codec context copy failed.");
		return -1;
	}

	// open codec (don't use directly from cc_orig)
	if (avcodec_open2(cc, codec, 0) < 0)
	{
		MsgBox("Couldn't open codec.");
		return -1;
	}

	// frames
	AVFrame *frame = av_frame_alloc();
	AVFrame *frame_rgb = av_frame_alloc();

	if (!frame || !frame_rgb) { MsgBox("Couldn't allocate frame."); return -1; }


	// mem for frame
	int numBytes = avpicture_get_size(AV_PIX_FMT_RGB24, cc->width, cc->height);
	u8 *buffer = (u8 *)av_malloc(numBytes * sizeof(u8));


	// tie frame and buffer together i think
	avpicture_fill((AVPicture *)frame_rgb, buffer, AV_PIX_FMT_RGB24, cc->width, cc->height);


	struct SwsContext *sws_context = 0;
	AVPacket packet;

	// init sws context (sws = software scaling?)
	sws_context= sws_getContext(
         cc->width,
         cc->height,
         cc->pix_fmt,
         cc->width,
         cc->height,
         AV_PIX_FMT_RGB24,
         SWS_BILINEAR,
         0, 0, 0);

	int count = 0;
	while (av_read_frame(fc, &packet) >= 0)
	{
		if (packet.stream_index == videoStream)
		{
			int frame_finished;
			avcodec_decode_video2(cc, frame, &frame_finished, &packet);

			if (frame_finished)
			{
				sws_scale(
	          		sws_context,
	          		(u8 const * const *)frame->data, frame->linesize,
		          	0, cc->height,
		          	frame_rgb->data, frame_rgb->linesize);
			}

			if (count < 5)
			{
				SaveFrame(frame_rgb, cc->width, cc->height, count);
				count++;
			}
		}
	}


	return 1;
}



