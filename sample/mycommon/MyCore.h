

typedef enum { FALSE, TRUE }  Bool;

typedef struct 
{
	int x;
	int y;
}TPoint;

typedef struct
{
	char ip[16];
	char IPMask[16];
	char Gayway[16];
	TPoint p1[2];	
	TPoint p2[2];
	TPoint p3[2];
	TPoint p4[2];	
	int angel[2];
	int width;
	int t1,t2;
	TPoint pjs1[2];	
	TPoint pjs2[2];
	int widthjs;
	int CamWidthjs;	
	int Ybegin;
	int Yend;
	int Ttimejs;//ͣ��ʱ��
	
	TPoint p5[2];
	TPoint p6[2];
	TPoint p7[2];
	TPoint p8[2];
	int XueShengStatus;//1 �� 0 ��
	int JSBK;//0 ����¼����  1���汳��
	int UpDownMode;//0 ֻȡ��λ  1�ߵ��л�
} TSetting; 

typedef struct
{
	int x[2];//,y[2];
	int w[2];//,h[2];
	int id;
}TBlob;

typedef struct
{
	int x1;
	int x2;
	int ps;
}TBlobShow;

typedef struct
{
	int x;
	int w;
	int id;
}TBlobSimple;

typedef struct
{
	int arc[4];
	int num;
}TArcSimple;

typedef struct
{
	int pointnum;//��Ч������Ŀ
	int x1[2];
	int x2[2];
}TPointResult;


