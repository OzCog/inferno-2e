#include <lib9.h>

/*
 * disassemble PowerPC opcodes in Plan9 format
 *	charles@vitanuova.com
 */

/*
 * ibm conventions for these: bit 0 is top bit
 *	from table 10-1
 */
typedef struct {
	uchar	aa;		/* bit 30 */
	uchar	crba;		/* bits 11-15 */
	uchar	crbb;		/* bits 16-20 */
	long	bd;		/* bits 16-29 */
	uchar	crfd;		/* bits 6-8 */
	uchar	crfs;		/* bits 11-13 */
	uchar	bi;		/* bits 11-15 */
	uchar	bo;		/* bits 6-10 */
	uchar	crbd;		/* bits 6-10 */
	union {
		short	d;	/* bits 16-31 */
		short	simm;
		ushort	uimm;
	};
	uchar	fm;		/* bits 7-14 */
	uchar	fra;		/* bits 11-15 */
	uchar	frb;		/* bits 16-20 */
	uchar	frc;		/* bits 21-25 */
	uchar	frs;		/* bits 6-10 */
	uchar	frd;		/* bits 6-10 */
	uchar	crm;		/* bits 12-19 */
	long	li;		/* bits 6-29 || b'00' */
	uchar	lk;		/* bit 31 */
	uchar	mb;		/* bits 21-25 */
	uchar	me;		/* bits 26-30 */
	uchar	nb;		/* bits 16-20 */
	uchar	op;		/* bits 0-5 */
	uchar	oe;		/* bit 21 */
	uchar	ra;		/* bits 11-15 */
	uchar	rb;		/* bits 16-20 */
	uchar	rc;		/* bit 31 */
	union {
		uchar	rs;	/* bits 6-10 */
		uchar	rd;
	};
	uchar	sh;		/* bits 16-20 */
	ushort	spr;		/* bits 11-20 */
	uchar	to;		/* bits 6-10 */
	uchar	imm;		/* bits 16-19 */
	ushort	xo;		/* bits 21-30, 22-30, 26-30, or 30 (beware) */
	long	immediate;
	long w0;
	long w1;
	ulong	addr;		/* pc of instruction */
	short	target;
	char	*curr;		/* current fill level in output buffer */
	char	*end;		/* end of buffer */
	int 	size;		/* number of longs in instr */
	char	*err;		/* errmsg */
} Instr;

#define	IBF(v,a,b) (((ulong)(v)>>(32-(b)-1)) & ~(~0L<<(((b)-(a)+1))))
#define	IB(v,b) IBF((v),(b),(b))

static void
bprint(Instr *i, char *fmt, ...)
{
	va_list arg;

	va_start(arg, fmt);
	i->curr = doprint(i->curr, i->end, fmt, arg);
	va_end(arg);
}

static int
decode(ulong *pc, Instr *i)
{
	ulong w;

	w = *pc;
	i->aa = IB(w, 30);
	i->crba = IBF(w, 11, 15);
	i->crbb = IBF(w, 16, 20);
	i->bd = IBF(w, 16, 29)<<2;
	if(i->bd & 0x8000)
		i->bd |= ~0L<<16;
	i->crfd = IBF(w, 6, 8);
	i->crfs = IBF(w, 11, 13);
	i->bi = IBF(w, 11, 15);
	i->bo = IBF(w, 6, 10);
	i->crbd = IBF(w, 6, 10);
	i->uimm = IBF(w, 16, 31);	/* also d, simm */
	i->fm = IBF(w, 7, 14);
	i->fra = IBF(w, 11, 15);
	i->frb = IBF(w, 16, 20);
	i->frc = IBF(w, 21, 25);
	i->frs = IBF(w, 6, 10);
	i->frd = IBF(w, 6, 10);
	i->crm = IBF(w, 12, 19);
	i->li = IBF(w, 6, 29)<<2;
	if(IB(w, 6))
		i->li |= ~0<<25;
	i->lk = IB(w, 31);
	i->mb = IBF(w, 21, 25);
	i->me = IBF(w, 26, 30);
	i->nb = IBF(w, 16, 20);
	i->op = IBF(w, 0, 5);
	i->oe = IB(w, 21);
	i->ra = IBF(w, 11, 15);
	i->rb = IBF(w, 16, 20);
	i->rc = IB(w, 31);
	i->rs = IBF(w, 6, 10);	/* also rd */
	i->sh = IBF(w, 16, 20);
	i->spr = IBF(w, 11, 20);
	i->to = IBF(w, 6, 10);
	i->imm = IBF(w, 16, 19);
	i->xo = IBF(w, 21, 30);		/* bits 21-30, 22-30, 26-30, or 30 (beware) */
	i->immediate = i->simm;
	if(i->op == 15)
		i->immediate <<= 16;
	i->w0 = w;
	i->target = -1;
	i->addr = (ulong)pc;
	i->size = 1;
	return 1;
}

static int
mkinstr(ulong *pc, Instr *i)
{
	Instr x;

	if(decode(pc, i) < 0)
		return -1;
	/*
	 * combine ADDIS/ORI (CAU/ORIL) into MOVW
	 */
	if (i->op == 15 && i->ra==0) {
		if(decode(pc+1, &x) < 0)
			return -1;
		if (x.op == 24 && x.rs == x.ra && x.ra == i->rd) {
			i->immediate |= (x.immediate & 0xFFFF);
			i->w1 = x.w0;
			i->target = x.rd;
			i->size++;
			return 1;
		}
	}
	return 1;
}

static void
pglobal(Instr *i, long off, char *reg)
{
	bprint(i, "%lux%s", off, reg);
}

static void
address(Instr *i)
{
	if(i->simm < 0)
		bprint(i, "-%lx(R%d)", -i->simm, i->ra);
	else
		bprint(i, "%lux(R%d)", i->immediate, i->ra);
}

static	char	*tcrbits[] = {"LT", "GT", "EQ", "VS"};
static	char	*fcrbits[] = {"GE", "LE", "NE", "VC"};

typedef struct Opcode Opcode;

struct Opcode {
	uchar	op;
	ushort	xo;
	ushort	xomask;
	char	*mnemonic;
	void	(*f)(Opcode *, Instr *);
	char	*ken;
	int	flags;
};

static void format(char *, Instr *, char *);

static void
branch(Opcode *o, Instr *i)
{
	char buf[8];
	int bo, bi;

	bo = i->bo & ~1;	/* ignore prediction bit */
	if(bo==4 || bo==12 || bo==20) {	/* simple forms */
		if(bo != 20) {
			bi = i->bi&3;
			sprint(buf, "B%s%%L", bo==12? tcrbits[bi]: fcrbits[bi]);
			format(buf, i, 0);
			bprint(i, "\t");
			if(i->bi > 4)
				bprint(i, "CR(%d),", i->bi/4);
		} else
			format("BR%L\t", i, 0);
		if(i->op == 16)
			format(0, i, "%J");
		else if(i->op == 19 && i->xo == 528)
			format(0, i, "(CTR)");
		else if(i->op == 19 && i->xo == 16)
			format(0, i, "(LR)");
	} else
		format(o->mnemonic, i, o->ken);
}

static void
addi(Opcode *o, Instr *i)
{
	if (i->op==14 && i->ra == 0)
		format("MOVW", i, "%i,R%d");
	else if(i->op==14 && i->simm < 0) {
		bprint(i, "SUB\t$%d,R%d", -i->simm, i->ra);
		if(i->rd != i->ra)
			bprint(i, ",R%d", i->rd);
	} else if(i->ra == i->rd) {
		format(o->mnemonic, i, "%i");
		bprint(i, ",R%d", i->rd);
	} else
		format(o->mnemonic, i, o->ken);
}

static void
addis(Opcode *o, Instr *i)
{
	long v;

	v = i->immediate;
	if (i->op==15 && i->ra == 0)
		bprint(i, "MOVW\t$%lux,R%d", v, i->rd);
	else if(i->op==15 && v < 0) {
		bprint(i, "SUB\t$%d,R%d", -v, i->ra);
		if(i->rd != i->ra)
			bprint(i, ",R%d", i->rd);
	} else {
		format(o->mnemonic, i, 0);
		bprint(i, "\t$%ld,R%d", v, i->ra);
		if(i->rd != i->ra)
			bprint(i, ",R%d", i->rd);
	}
}

static void
andi(Opcode *o, Instr *i)
{
	if (i->ra == i->rs)
		format(o->mnemonic, i, "%I,R%d");
	else
		format(o->mnemonic, i, o->ken);
}

static void
gencc(Opcode *o, Instr *i)
{
	format(o->mnemonic, i, o->ken);
}

static void
gen(Opcode *o, Instr *i)
{
	format(o->mnemonic, i, o->ken);
	if (i->rc)
		bprint(i, " [illegal Rc]");
}

static void
ldx(Opcode *o, Instr *i)
{
	if(i->ra == 0)
		format(o->mnemonic, i, "(R%b),R%d");
	else
		format(o->mnemonic, i, "(R%b+R%a),R%d");
	if(i->rc)
		bprint(i, " [illegal Rc]");
}

static void
stx(Opcode *o, Instr *i)
{
	if(i->ra == 0)
		format(o->mnemonic, i, "R%d,(R%b)");
	else
		format(o->mnemonic, i, "R%d,(R%b+R%a)");
	if(i->rc && i->xo != 150)
		bprint(i, " [illegal Rc]");
}

static void
fldx(Opcode *o, Instr *i)
{
	if(i->ra == 0)
		format(o->mnemonic, i, "(R%b),F%d");
	else
		format(o->mnemonic, i, "(R%b+R%a),F%d");
	if(i->rc)
		bprint(i, " [illegal Rc]");
}

static void
fstx(Opcode *o, Instr *i)
{
	if(i->ra == 0)
		format(o->mnemonic, i, "F%d,(R%b)");
	else
		format(o->mnemonic, i, "F%d,(R%b+R%a)");
	if(i->rc)
		bprint(i, " [illegal Rc]");
}

static void
dcb(Opcode *o, Instr *i)
{
	if(i->ra == 0)
		format(o->mnemonic, i, "(R%b)");
	else
		format(o->mnemonic, i, "(R%b+R%a)");
	if(i->rd)
		bprint(i, " [illegal Rd]");
	if(i->rc)
		bprint(i, " [illegal Rc]");
}

static void
lw(Opcode *o, Instr *i, char r)
{
	bprint(i, "%s\t", o->mnemonic);
	address(i);
	bprint(i, ",%c%d", r, i->rd);
}

static void
load(Opcode *o, Instr *i)
{
	lw(o, i, 'R');
}

static void
fload(Opcode *o, Instr *i)
{
	lw(o, i, 'F');
}

static void
sw(Opcode *o, Instr *i, char r)
{
	char *m;

	m = o->mnemonic;
	if (r == 'F')
		format(m, i, "F%d,%l");
	else
		format(m, i, o->ken);
}

static void
store(Opcode *o, Instr *i)
{
	sw(o, i, 'R');
}

static void
fstore(Opcode *o, Instr *i)
{
	sw(o, i, 'F');
}

static void
shifti(Opcode *o, Instr *i)
{
	if (i->ra == i->rs)
		format(o->mnemonic, i, "$%k,R%a");
	else
		format(o->mnemonic, i, o->ken);
}

static void
shift(Opcode *o, Instr *i)
{
	if (i->ra == i->rs)
		format(o->mnemonic, i, "R%b,R%a");
	else
		format(o->mnemonic, i, o->ken);
}

static void
add(Opcode *o, Instr *i)
{
	if (i->rd == i->ra)
		format(o->mnemonic, i, "R%b,R%d");
	else if (i->rd == i->rb)
		format(o->mnemonic, i, "R%a,R%d");
	else
		format(o->mnemonic, i, o->ken);
}

static void
sub(Opcode *o, Instr *i)
{
	format(o->mnemonic, i, 0);
	bprint(i, "\t");
	if(i->op == 31) {
		bprint(i, "\tR%d,R%d", i->ra, i->rb);	/* subtract Ra from Rb */
		if(i->rd != i->rb)
			bprint(i, ",R%d", i->rd);
	} else
		bprint(i, "\tR%d,$%d,R%d", i->ra, i->simm, i->rd);
}

static void
div(Opcode *o, Instr *i)
{
	format(o->mnemonic, i, 0);
	if(i->op == 31)
		bprint(i, "\tR%d,R%d", i->rb, i->ra);
	else
		bprint(i, "\t$%d,R%d", i->simm, i->ra);
	if(i->ra != i->rd)
		bprint(i, ",R%d", i->rd);
}

static void
and(Opcode *o, Instr *i)
{
	if (i->op == 31) {
		/* Rb,Rs,Ra */
		if (i->ra == i->rs)
			format(o->mnemonic, i, "R%b,R%a");
		else if (i->ra == i->rb)
			format(o->mnemonic, i, "R%s,R%a");
		else
			format(o->mnemonic, i, o->ken);
	} else {
		/* imm,Rs,Ra */
		if (i->ra == i->rs)
			format(o->mnemonic, i, "%I,R%a");
		else
			format(o->mnemonic, i, o->ken);
	}
}

static void
or(Opcode *o, Instr *i)
{
	if (i->op == 31) {
		/* Rb,Rs,Ra */
		if (i->rs == 0 && i->ra == 0 && i->rb == 0)
			format("NOP", i, 0);
		else if (i->rs == i->rb)
			format("MOVW", i, "R%b,R%a");
		else
			and(o, i);
	} else
		and(o, i);
}

static void
shifted(Opcode *o, Instr *i)
{
	format(o->mnemonic, i, 0);
	bprint(i, "\t$%lux,", (ulong)i->uimm<<16);
	if (i->rs == i->ra)
		bprint(i, "R%d", i->ra);
	else
		bprint(i, "R%d,R%d", i->rs, i->ra);
}

static void
neg(Opcode *o, Instr *i)
{
	if (i->rd == i->ra)
		format(o->mnemonic, i, "R%d");
	else
		format(o->mnemonic, i, o->ken);
}

static	char	ir2[] = "R%a,R%d";		/* reverse of IBM order */
static	char	ir3[] = "R%b,R%a,R%d";
static	char	ir3r[] = "R%a,R%b,R%d";
static	char	il3[] = "R%b,R%s,R%a";
static	char	il2u[] = "%I,R%s,R%a";
static	char	il3s[] = "$%k,R%s,R%a";
static	char	il2[] = "R%s,R%a";
static	char	icmp3[] = "R%a,R%b,%D";
static	char	cr3op[] = "%b,%a,%d";
static	char	ir2i[] = "%i,R%a,R%d";
static	char	fp2[] = "F%b,F%d";
static	char	fp3[] = "F%b,F%a,F%d";
static	char	fp3c[] = "F%c,F%a,F%d";
static	char	fp4[] = "F%a,F%c,F%b,F%d";
static	char	fpcmp[] = "F%a,F%b,%D";
static	char	ldop[] = "%l,R%d";
static	char	stop[] = "R%d,%l";
static	char	fldop[] = "%l,F%d";
static	char	fstop[] = "F%d,%l";
static	char	rlim[] = "R%b,R%s,$%z,R%a";
static	char	rlimi[] = "$%k,R%s,$%z,R%a";

#define	OEM	IBF(~0,22,30)
#define	FP4	IBF(~0,26,30)
#define	ALL	(~0)
/*
notes:
	10-26: crfD = rD>>2; rD&3 mbz
		also, L bit (bit 10) mbz or selects 64-bit operands
*/

static Opcode opcodes[] = {
	{31,	266,	OEM,	"ADD%V%C",	add,	ir3},
	{31,	 10,	OEM,	"ADDC%V%C",	add,	ir3},
	{31,	138,	OEM,	"ADDE%V%C",	add,	ir3},
	{14,	0,	0,	"ADD",		addi,	ir2i},
	{12,	0,	0,	"ADDC",		addi,	ir2i},
	{13,	0,	0,	"ADDCCC",	addi,	ir2i},
	{15,	0,	0,	"ADD",		addis,	0},
	{31,	234,	OEM,	"ADDME%V%C",	gencc,	ir2},
	{31,	202,	OEM,	"ADDZE%V%C",	gencc,	ir2},

	{31,	28,	ALL,	"AND%C",	and,	il3},
	{31,	60,	ALL,	"ANDN%C",	and,	il3},
	{28,	0,	0,	"ANDCC",		andi,	il2u},
	{29,	0,	0,	"ANDCC",		shifted, 0},

	{18,	0,	0,	"B%L",		gencc,	"%j"},
	{16,	0,	0,	"BC%L",		branch,	"%d,%a,%J"},
	{19,	528,	ALL,	"BC%L",		branch,	"%d,%a,(CTR)"},
	{19,	16,	ALL,	"BC%L",		branch,	"%d,%a,(LR)"},

	{31,	0,	ALL,	"CMP",		0,	icmp3},
	{11,	0,	0,	"CMP",		0,	"R%a,%i,%D"},
	{31,	32,	ALL,	"CMPU",		0,	icmp3},
	{10,	0,	0,	"CMPU",		0,	"R%a,%I,%D"},

	{31,	26,	ALL,	"CNTLZ%C",	gencc,	ir2},

	{19,	257,	ALL,	"CRAND",	gen,	cr3op},
	{19,	129,	ALL,	"CRANDN",	gen,	cr3op},
	{19,	289,	ALL,	"CREQV",	gen,	cr3op},
	{19,	225,	ALL,	"CRNAND",	gen,	cr3op},
	{19,	33,	ALL,	"CRNOR",	gen,	cr3op},
	{19,	449,	ALL,	"CROR",		gen,	cr3op},
	{19,	417,	ALL,	"CRORN",	gen,	cr3op},
	{19,	193,	ALL,	"CRXOR",	gen,	cr3op},

	{31,	86,	ALL,	"DCBF",		dcb,	0},
	{31,	470,	ALL,	"DCBI",		dcb,	0},
	{31,	54,	ALL,	"DCBST",	dcb,	0},
	{31,	278,	ALL,	"DCBT",		dcb,	0},
	{31,	246,	ALL,	"DCBTST",	dcb,	0},
	{31,	1014,	ALL,	"DCBZ",		dcb,	0},

	{31,	491,	OEM,	"DIVW%V%C",	div,	ir3},
	{31,	459,	OEM,	"DIVWU%V%C",	div,	ir3},

	{31,	310,	ALL,	"ECIWX",	ldx,	0},
	{31,	438,	ALL,	"ECOWX",	stx,	0},
	{31,	854,	ALL,	"EIEIO",	gen,	0},

	{31,	284,	ALL,	"EQV%C",	gencc,	il3},

	{31,	954,	ALL,	"EXTSB%C",	gencc,	il2},
	{31,	922,	ALL,	"EXTSH%C",	gencc,	il2},

	{63,	264,	ALL,	"FABS%C",	gencc,	fp2},
	{63,	21,	ALL,	"FADD%C",	gencc,	fp3},
	{59,	21,	ALL,	"FADDS%C",	gencc,	fp3},
	{63,	32,	ALL,	"FCMPO",	gen,	fpcmp},
	{63,	0,	ALL,	"FCMPU",	gen,	fpcmp},
	{63,	14,	ALL,	"FCTIW%C",	gencc,	fp2},
	{63,	15,	ALL,	"FCTIWZ%C",	gencc,	fp2},
	{63,	18,	ALL,	"FDIV%C",	gencc,	fp3},
	{59,	18,	ALL,	"FDIVS%C",	gencc,	fp3},
	{63,	29,	FP4,	"FMADD%C",	gencc,	fp4},
	{59,	29,	FP4,	"FMADDS%C",	gencc,	fp4},
	{63,	72,	ALL,	"FMOVD%C",	gencc,	fp2},
	{63,	28,	FP4,	"FMSUB%C",	gencc,	fp4},
	{59,	28,	FP4,	"FMSUBS%C",	gencc,	fp4},
	{63,	25,	FP4,	"FMUL%C",	gencc,	fp3c},
	{59,	25,	FP4,	"FMULS%C",	gencc,	fp3c},
	{63,	136,	ALL,	"FNABS%C",	gencc,	fp2},
	{63,	40,	ALL,	"FNEG%C",	gencc,	fp2},
	{63,	31,	FP4,	"FNMADD%C",	gencc,	fp4},
	{59,	31,	FP4,	"FNMADDS%C",	gencc,	fp4},
	{63,	30,	FP4,	"FNMSUB%C",	gencc,	fp4},
	{59,	30,	FP4,	"FNMSUBS%C",	gencc,	fp4},
	{63,	12,	ALL,	"FRSP%C",	gencc,	fp2},
	{63,	20,	FP4,	"FSUB%C",	gencc,	fp3},
	{59,	20,	FP4,	"FSUBS%C",	gencc,	fp3},

	{31,	982,	ALL,	"ICBI",		dcb,	0},
	{19,	150,	ALL,	"ISYNC",	gen,	0},

	{34,	0,	0,	"MOVBZ",	load,	ldop},
	{35,	0,	0,	"MOVBZU",	load,	ldop},
	{31,	119,	ALL,	"MOVBZU",	ldx,	0},
	{31,	87,	ALL,	"MOVBZ",	ldx,	0},
	{50,	0,	0,	"FMOVD",	fload,	fldop},
	{51,	0,	0,	"FMOVDU",	fload,	fldop},
	{31,	631,	ALL,	"FMOVDU",	fldx,	0},
	{31,	599,	ALL,	"FMOVD",	fldx,	0},
	{48,	0,	0,	"FMOVS",	load,	fldop},
	{49,	0,	0,	"FMOVSU",	load,	fldop},
	{31,	567,	ALL,	"FMOVSU",	fldx,	0},
	{31,	535,	ALL,	"FMOVS",	fldx,	0},
	{42,	0,	0,	"MOVH",		load,	ldop},
	{43,	0,	0,	"MOVHU",	load,	ldop},
	{31,	375,	ALL,	"MOVHU",	ldx,	0},
	{31,	343,	ALL,	"MOVH",		ldx,	0},
	{31,	790,	ALL,	"MOVHBR",	ldx,	0},
	{40,	0,	0,	"MOVHZ",	load,	ldop},
	{41,	0,	0,	"MOVHZU",	load,	ldop},
	{31,	311,	ALL,	"MOVHZU",	ldx,	0},
	{31,	279,	ALL,	"MOVHZ",	ldx,	0},
	{46,	0,	0,	"MOVMW",		load,	ldop},
	{31,	597,	ALL,	"LSW",		gen,	"(R%a),$%n,R%d"},
	{31,	533,	ALL,	"LSW",		ldx,	0},
	{31,	20,	ALL,	"LWAR",		ldx,	0},
	{31,	534,	ALL,	"MOVWBR",		ldx,	0},
	{32,	0,	0,	"MOVW",		load,	ldop},
	{33,	0,	0,	"MOVWU",	load,	ldop},
	{31,	55,	ALL,	"MOVWU",	ldx,	0},
	{31,	23,	ALL,	"MOVW",		ldx,	0},

	{19,	0,	ALL,	"MOVFL",	gen,	"%S,%D"},
	{63,	64,	ALL,	"MOVCRFS",	gen,	"%S,%D"},
	{31,	512,	ALL,	"MOVW",	gen,	"XER,%D"},
	{31,	19,	ALL,	"MOVW",	gen,	"CR,R%d"},

	{63,	583,	ALL,	"MOVW%C",	gen,	"FPSCR, F%d"},	/* mffs */
	{31,	83,	ALL,	"MOVW",		gen,	"MSR,R%d"},
	{31,	339,	ALL,	"MOVW",		gen,	"%P,R%d"},
	{31,	595,	ALL,	"MOVW",		gen,	"SEG(%a),R%d"},
	{31,	659,	ALL,	"MOVW",		gen,	"SEG(R%b),R%d"},
	{31,	144,	ALL,	"MOVFL",	gen,	"R%s,%m,CR"},
	{63,	70,	ALL,	"MTFSB0%C",	gencc,	"%D"},
	{63,	38,	ALL,	"MTFSB1%C",	gencc,	"%D"},
	{63,	711,	ALL,	"MOVFL%C",	gencc,	"F%b,%M,FPSCR"},	/* mtfsf */
	{63,	134,	ALL,	"MOVFL%C",	gencc,	"%K,%D"},
	{31,	146,	ALL,	"MOVW",		gen,	"R%s,MSR"},
	{31,	467,	ALL,	"MOVW",		gen,	"R%s,%P"},
	{31,	210,	ALL,	"MOVW",		gen,	"R%s,SEG(%a)"},
	{31,	242,	ALL,	"MOVW",		gen,	"R%s,SEG(R%b)"},

	{31,	235,	OEM,	"MULLW%V%C",	gencc,	ir3},
	{7,	0,	0,	"MULLW",	div,	"%i,R%a,R%d"},

	{31,	476,	ALL,	"NAND%C",	gencc,	il3},
	{31,	104,	OEM,	"NEG%V%C",	neg,	ir2},
	{31,	124,	ALL,	"NOR%C",	gencc,	il3},
	{31,	444,	ALL,	"OR%C",	or,	il3},
	{31,	412,	ALL,	"ORN%C",	or,	il3},
	{24,	0,	0,	"OR",		and,	"%I,R%d,R%a"},
	{25,	0,	0,	"OR",		shifted, 0},

	{19,	50,	ALL,	"RFI",		gen,	0},

	{20,	0,	0,	"RLWMI%C",	gencc,	rlimi},
	{21,	0,	0,	"RLWNM%C",	gencc,	rlimi},
	{23,	0,	0,	"RLWNM%C",	gencc,	rlim},

	{17,	1,	ALL,	"SYSCALL",	gen,	0},

	{31,	24,	ALL,	"SLW%C",	shift,	il3},

	{31,	792,	ALL,	"SRAW%C",	shift,	il3},
	{31,	824,	ALL,	"SRAW%C",	shifti,	il3s},

	{31,	536,	ALL,	"SRW%C",	shift,	il3},

	{38,	0,	0,	"MOVB",		store,	stop},
	{39,	0,	0,	"MOVBU",	store,	stop},
	{31,	247,	ALL,	"MOVBU",	stx,	0},
	{31,	215,	ALL,	"MOVB",		stx,	0},
	{54,	0,	0,	"FMOVD",	fstore,	fstop},
	{55,	0,	0,	"FMOVDU",	fstore,	fstop},
	{31,	759,	ALL,	"FMOVDU",	fstx,	0},
	{31,	727,	ALL,	"FMOVD",	fstx,	0},
	{52,	0,	0,	"FMOVS",	fstore,	fstop},
	{53,	0,	0,	"FMOVSU",	fstore,	fstop},
	{31,	695,	ALL,	"FMOVSU",	fstx,	0},
	{31,	663,	ALL,	"FMOVS",	fstx,	0},
	{44,	0,	0,	"MOVH",		store,	stop},
	{31,	918,	ALL,	"MOVHBR",	stx,	0},
	{45,	0,	0,	"MOVHU",	store,	stop},
	{31,	439,	ALL,	"MOVHU",	stx,	0},
	{31,	407,	ALL,	"MOVH",		stx,	0},
	{47,	0,	0,	"MOVMW",		store,	stop},
	{31,	725,	ALL,	"STSW",		gen,	"R%d,$%n,(R%a)"},
	{31,	661,	ALL,	"STSW",		stx,	0},
	{36,	0,	0,	"MOVW",		store,	stop},
	{31,	662,	ALL,	"MOVWBR",	stx,	0},
	{31,	150,	ALL,	"STWCCC",		stx,	0},
	{37,	0,	0,	"MOVWU",	store,	stop},
	{31,	183,	ALL,	"MOVWU",	stx,	0},
	{31,	151,	ALL,	"MOVW",		stx,	0},

	{31,	40,	OEM,	"SUB%V%C",	sub,	ir3},
	{31,	8,	OEM,	"SUBC%V%C",	sub,	ir3},
	{31,	136,	OEM,	"SUBE%V%C",	sub,	ir3},
	{8,	0,	0,	"SUBC",		gen,	"R%a,%i,R%d"},
	{31,	232,	OEM,	"SUBME%V%C",	sub,	ir2},
	{31,	200,	OEM,	"SUBZE%V%C",	sub,	ir2},

	{31,	598,	ALL,	"SYNC",		gen,	0},
	{31,	306,	ALL,	"TLBIE",	gen,	"R%b"},
	{31,	1010,	ALL,	"TLBLI",	gen,	"R%b"},
	{31,	978,	ALL,	"TLBLD",	gen,	"R%b"},
	{31,	4,	ALL,	"TW",		gen,	"%d,R%a,R%b"},
	{3,	0,	0,	"TW",		gen,	"%d,R%a,%i"},

	{31,	316,	ALL,	"XOR",		and,	il3},
	{26,	0,	0,	"XOR",		and,	il2u},
	{27,	0,	0,	"XOR",		shifted, 0},

	{0},
};

typedef struct Spr Spr;
struct Spr {
	int	n;
	char	*name;
};

static	Spr	sprname[] = {
	{0, "MQ"},
	{1, "XER"},
	{268, "TBL"},
	{269, "TBU"},
	{8, "LR"},
	{9, "CTR"},
	{528, "IBAT0U"},
	{529, "IBAT0L"},
	{530, "IBAT1U"},
	{531, "IBAT1L"},
	{532, "IBAT2U"},
	{533, "IBAT2L"},
	{534, "IBAT3U"},
	{535, "IBAT3L"},
	{536, "DBAT0U"},
	{537, "DBAT0L"},
	{538, "DBAT1U"},
	{539, "DBAT1L"},
	{540, "DBAT2U"},
	{541, "DBAT2L"},
	{542, "DBAT3U"},
	{543, "DBAT3L"},
	{25, "SDR1"},
	{19, "DAR"},
	{272, "SPRG0"},
	{273, "SPRG1"},
	{274, "SPRG2"},
	{275, "SPRG3"},
	{18, "DSISR"},
	{26, "SRR0"},
	{27, "SRR1"},
	{284, "TBLW"},
	{285, "TBUW"},	
	{22, "DEC"},
	{282, "EAR"},
	{1008, "HID0"},
	{1009, "HID1"},
	{976, "DMISS"},
	{977, "DCMP"},
	{978, "HASH1"},
	{979, "HASH2"},
	{980, "IMISS"},
	{981, "ICMP"},
	{982, "RPA"},
	{1010, "IABR"},
	{0,0},
};

static void
format(char *mnemonic, Instr *i, char *f)
{
	int n, s;
	ulong mask;

	if (mnemonic)
		format(0, i, mnemonic);
	if (f == 0)
		return;
	if (mnemonic)
		bprint(i, "\t");
	for ( ; *f; f++) {
		if (*f != '%') {
			bprint(i, "%c", *f);
			continue;
		}
		switch (*++f) {
		case 'V':
			if(i->oe)
				bprint(i, "V");
			break;

		case 'C':
			if(i->rc)
				bprint(i, "CC");
			break;

		case 'a':
			bprint(i, "%d", i->ra);
			break;

		case 'b':
			bprint(i, "%d", i->rb);
			break;

		case 'c':
			bprint(i, "%d", i->frc);
			break;

		case 'd':
		case 's':
			bprint(i, "%d", i->rd);
			break;

		case 'S':
			if(i->ra & 3)
				bprint(i, "CR(INVAL:%d)", i->ra);
			else if(i->op == 63)
				bprint(i, "FPSCR(%d)", i->crfs);
			else
				bprint(i, "CR(%d)", i->crfs);
			break;

		case 'D':
			if(i->rd & 3)
				bprint(i, "CR(INVAL:%d)", i->rd);
			else if(i->op == 63)
				bprint(i, "FPSCR(%d)", i->crfd);
			else
				bprint(i, "CR(%d)", i->crfd);
			break;

		case 'l':
			if(i->simm < 0)
				bprint(i, "-%lx(R%d)", -i->simm, i->ra);
			else
				bprint(i, "%lx(R%d)", i->simm, i->ra);
			break;

		case 'i':
			bprint(i, "$%ld", i->simm);
			break;

		case 'I':
			bprint(i, "$%lx", i->uimm);
			break;

		case 'w':
			bprint(i, "[%lux]", i->w0);
			break;

		case 'P':
			n = ((i->spr&0x1f)<<5)|((i->spr>>5)&0x1f);
			for(s=0; sprname[s].name; s++)
				if(sprname[s].n == n)
					break;
			if(sprname[s].name) {
				if(n < 10)
					bprint(i, sprname[s].name);
				else
					bprint(i, "SPR(%s)", sprname[s].name);
			} else
				bprint(i, "SPR(%d)", n);
			break;

		case 'n':
			bprint(i, "%d", i->nb==0? 32: i->nb);	/* eg, pg 10-103 */
			break;

		case 'm':
			bprint(i, "%lx", i->crm);
			break;

		case 'M':
			bprint(i, "%lx", i->fm);
			break;

		case 'z':
			if(i->mb <= i->me)
				mask = ((ulong)~0L>>i->mb) & (~0L<<(31-i->me));
			else
				mask = ~(((ulong)~0L>>(i->me+1)) & (~0L<<(31-(i->mb-1))));
			bprint(i, "%lux", mask);
			break;

		case 'k':
			bprint(i, "%d", i->sh);
			break;

		case 'K':
			bprint(i, "$%x", i->imm);
			break;

		case 'L':
			if(i->lk)
				bprint(i, "L");
			break;

		case 'j':
			if(i->aa)
				pglobal(i, i->li, "(ABS)");
			else
				pglobal(i, i->addr+i->li, "(REL)");
			break;

		case 'J':
			if(i->aa)
				pglobal(i, i->bd, "(ABS)");
			else
				pglobal(i, i->addr+i->bd, "(REL)");
			break;

		case '\0':
			bprint(i, "%%");
			return;

		default:
			bprint(i, "%%%c", *f);
			break;
		}
	}
}

int
das(ulong *pc)
{
	Instr i;
	Opcode *o;
	char buf[100];
	int r;

	memset(&i, 0, sizeof(i));
	i.curr = buf;
	i.end = buf+sizeof(buf)-1;
	r = mkinstr(pc, &i);
	i.curr += sprint(i.curr, "	%.8lux %.8lux ", pc, i.w0);
	if(r >= 0){
		if(i.size == 2)
			i.curr += sprint(i.curr, "%.8lux ", i.w1);
		for(o = opcodes; o->mnemonic != 0; o++)
			if(i.op == o->op && (i.xo & o->xomask) == o->xo) {
				if (o->f)
					(*o->f)(o, &i);
				else
					format(o->mnemonic, &i, o->ken);
				print("%s\n", buf);
				return i.size;
			}
	}
	strcpy(i.curr, "ILLEGAL");
	print("%s\n", buf);
	return i.size;
}
