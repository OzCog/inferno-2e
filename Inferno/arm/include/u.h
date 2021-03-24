#define nil		((void*)0)
typedef	unsigned short	ushort;
typedef	unsigned char	uchar;
typedef	signed char	schar;
typedef unsigned int	uint;
typedef unsigned long	ulong;
typedef	 long long	vlong;
typedef	unsigned long long uvlong;
typedef union Length	Length;
typedef ushort		Rune;

union Length
{
	char	clength[8];
	vlong	vlength;
	struct{
		long	hlength;
		long	length;
	};
};


typedef	char*	va_list;
#define va_start(list, start) list =\
	(sizeof(start)<4?\
		(char*)((int*)&(start)+1):\
		(char*)(&(start)+1))
#define va_end(list)
#define va_arg(list, mode)\
	((mode*)(list += sizeof(mode)))[-1]


/* FCR */
#define       FPINEX  (1<<20)
#define       FPUNFL  (1<<19)
#define       FPOVFL  (1<<18)
#define       FPZDIV  (1<<17)
#define       FPINVAL (1<<16)
#define       FPRNR   (0<<0)
#define       FPRZ    (1<<0)
#define       FPRPINF (2<<0)
#define       FPRNINF (3<<0)
#define       FPRMASK (3<<0)
#define       FPPEXT  0
#define       FPPSGL  0
#define       FPPDBL  0
#define       FPPMASK 0
/* FSR */
#define       FPAINEX (1<<4)
#define       FPAUNFL (1<<3)
#define       FPAOVFL (1<<2)
#define       FPAZDIV (1<<1)
#define       FPAINVAL        (1<<0)
