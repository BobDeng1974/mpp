/** ��ȡͼ��
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif // c++
#ifndef WIN32
#define __STDC_CONSTANT_MACROS
#define SUPPORT_YUANSDK
#endif
//#include <libswscale/swscale.h>

typedef struct imgsrc_t imgsrc_t;

typedef struct imgsrc_format
{
	PixelFormat fmt;
	int width;
	int height;
	double fps;
} imgsrc_format;


typedef struct zifImage
{
	PixelFormat fmt_type;
	int width;
	int height;
	unsigned char *data[4];
	int stride[4];

	double stamp;

	void *internal_ptr;
} zifImage;

/** url ֧����֡��ʽ��
		tcp://xxx zqpkt��ֱ���� ..
		yuan://N ֱ��ʹ��yuan sdk��ȡ N ͨ������ ..
		ipcam://.. ���� ip ����� ..
		rtsp://.. ���� rtsp Դ ..
 */
imgsrc_t *imgsrc_open(const char *url, const imgsrc_format *fmt);

/** ֧�� json ��ʽ������ format������������ python ���� ...
		{
			"pix_fmt": 3,
			"width": 960,
			"height": 540,
			"fps": 0.0
		}
 */
imgsrc_t *imgsrc_open_json(const char *url, const char *fmt_json_str);
void imgsrc_close(imgsrc_t *ctx);

/** ��ͣ/�ָ�:
		Ŀǰ����֧���ļ�
 */
int imgsrc_pause(imgsrc_t *ctx);
int imgsrc_resume(imgsrc_t *ctx);

/** ��ȡ��һ֡ͼ�����ڲ����峬��500msû��ͼ���򷵻� 0
	���������Чͼ��ʹ����ɺ󣬱������ imgsrc_free() �ͷţ�
 */
zifImage *imgsrc_next(imgsrc_t *ctx);
void imgsrc_free(imgsrc_t *ctx, zifImage *img);

#ifdef __cplusplus
}
#endif // c++
