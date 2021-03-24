include "sys.m";
include "math.m";
include "string.m";
include "bufio.m";
include "isa.m";

STemp:		con NREG * IBY2WD;
RTemp:		con STemp + IBY2WD;
DTemp:		con RTemp + IBY2WD;
MaxTemp:	con DTemp + IBY2WD;
MaxReg:		con 1 << 16;
MaxAlign:	con IBY2LG;
StrSize:	con 256;
MaxIncPath:	con 32;			# max directories in include path
MaxScope:	con 64;			# max nested {}
MaxInclude:	con 32;			# max nested include ""
ScopeBuiltin,
ScopeNils,
ScopeGlobal:	con iota;

Line:		type int;
PosBits:	con 10;
PosMask:	con (1 << PosBits) - 1;

Src: adt
{
	start:	Line;
	stop:	Line;
};

File: adt
{
	name:	string;
	abs:	int;				# absolute line of start of the part of file
	off:	int;				# offset to line in the file
	in:	int;				# absolute line where included
	act:	string;				# name of real file with #line fake file
	actoff:	int;				# offset from fake line to real line
	sbl:	int;				# symbol file number
};

Val: adt
{
	idval:	ref Sym;
	ival:	big;
	rval:	real;
};

Tok: adt
{
	src:	Src;
	v:	Val;
};

#
# addressing modes
#
	Aimm,					# immediate
	Amp,					# global
	Ampind,					# global indirect
	Afp,					# activation frame
	Afpind,					# frame indirect
	Apc,					# branch
	Adesc,					# type descriptor immediate
	Aoff,					# offset in module description table
	Aerr,					# error
	Anone,					# no operand
	Aend:		con byte iota;

Addr: adt
{
	reg:		int;
	offset:		int;
	decl:		cyclic ref Decl;
};

Inst: adt
{
	src:		Src;
	op:		int;			# could be a byte
	pc:		int;
	reach:		byte;			# could a control path reach this instruction?
	sm:		byte;			# operand addressing modes
	mm:		byte;
	dm:		byte;
	s:		cyclic Addr;		# operands
	m:		cyclic Addr;
	d:		cyclic Addr;
	branch:		cyclic ref Inst;	# branch destination
	next:		cyclic ref Inst;
	block:		int;			# blocks nested inside
};

Case: adt
{
	nlab:		int;
	nsnd:		int;
	offset:		int;			# offset in mp
	labs:		cyclic array of Label;
	wild:		cyclic ref Node;	# if nothing matches
	iwild:		cyclic ref Inst;
};

Label: adt
{
	node:		cyclic ref Node;
	isptr:		int;			# true if the labelled alt channel is a pointer
	start:		cyclic ref Node;	# value in range [start, stop) => code
	stop:		cyclic ref Node;
	inst:		cyclic ref Inst;
};

#
# storage classes
#
	Dtype,
	Dfn,
	Dglobal,
	Darg,
	Dlocal,
	Dconst,
	Dfield,
	Dtag,					# pick tags
	Dimport,				# imported identifier
	Dunbound,				# unbound identified
	Dundef,
	Dwundef,				# undefined, but don't whine

	Dend:		con iota;

Decl: adt
{
	src:		Src;			# where declaration
	sym:		cyclic ref Sym;		# name
	store:		int;			# storage class
	dot:		cyclic ref Decl;	# parent adt or module
	ty:		cyclic ref Type;
	refs:		int;			# number of references
	offset:		int;
	tag:		int;			# union tag

	scope:		int;			# in which it was declared
	next:		cyclic ref Decl;	# list in same scope, field or argument list, etc.
	old:		cyclic ref Decl;	# declaration of the symbol in enclosing scope

	eimport:	cyclic ref Node;	# expr from which imported
	importid:	cyclic ref Decl;	# identifier imported
	timport:	cyclic ref Decl;	# stack of identifiers importing a type

	init:		cyclic ref Node;	# data initialization
	tref:		int;			# 1 => is a tmp; >=2 => tmp in use
	cycle:		byte;			# can create a cycle
	cyc:		byte;			# so labelled in source
	cycerr:		byte;			# delivered an error message for cycle?
	implicit:	byte;			# implicit first argument in an adt?

	iface:		cyclic ref Decl;	# used external declarations in a module

	locals:		cyclic ref Decl;	# locals for a function
	pc:		cyclic ref Inst;	# start of function
	endpc:		cyclic ref Inst;	# limit of function

# should be able to move this to Type
	desc:		ref Desc;		# heap descriptor
};

Desc: adt
{
	id:		int;			# dis type identifier
	used:		int;			# actually used in output?
	map:		array of byte;		# byte map of pointers
	size:		int;			# length of the object
	nmap:		int;			# length of good bytes in map
	next:		cyclic ref Desc;
};

Sym: adt
{
	token:		int;
	name:		string;
	hash:		int;
	next:		cyclic ref Sym;
	decl:		cyclic ref Decl;
	unbound:	cyclic ref Decl;	# place holder for unbound symbols
};

#
# ops for nodes
#
	Oadd,
	Oaddas,
	Oadr,
	Oadtdecl,
	Oalt,
	Oand,
	Oandand,
	Oandas,
	Oarray,
	Oas,
	Obreak,
	Ocall,
	Ocase,
	Ocast,
	Ochan,
	Ocomp,
	Ocondecl,
	Ocons,
	Oconst,
	Ocont,
	Odas,
	Odec,
	Odiv,
	Odivas,
	Odo,
	Odot,
	Oelem,
	Oeq,
	Oexit,
	Ofielddecl,
	Ofor,
	Ofunc,
	Ogeq,
	Ogt,
	Ohd,
	Oif,
	Oimport,
	Oinc,
	Oind,
	Oindex,
	Oinds,
	Oindx,
	Ojmp,
	Olabel,
	Olen,
	Oleq,
	Oload,
	Olsh,
	Olshas,
	Olt,
	Omdot,
	Omod,
	Omodas,
	Omoddecl,
	Omul,
	Omulas,
	Oname,
	Oneg,
	Oneq,
	Onot,
	Onothing,
	Oor,
	Ooras,
	Ooror,
	Opick,
	Opickdecl,
	Opredec,
	Opreinc,
	Orange,
	Orcv,
	Oref,
	Oret,
	Orsh,
	Orshas,
	Oscope,
	Oseq,
	Oslice,
	Osnd,
	Ospawn,
	Osub,
	Osubas,
	Otagof,
	Otl,
	Otuple,
	Otypedecl,
	Oused,
	Ovardecl,
	Ovardecli,
	Owild,
	Oxor,
	Oxoras,

	Oend:		con iota + 1;

#
# moves
#
	Mas,
	Mcons,
	Mhd,
	Mtl,

	Mend:		con iota;

#
# addressability
#
	Rreg,				# v(fp)
	Rmreg,				# v(mp)
	Roff,				# $v
	Rdesc,				# $v
	Rconst,				# $v
	Ralways,			# preceeding are always addressable
	Radr,				# v(v(fp))
	Rmadr,				# v(v(mp))
	Rcant,				# following are not quite addressable
	Rpc,				# branch address
	Rmpc,				# cross module branch address
	Rareg,				# $v(fp)
	Ramreg,				# $v(mp)
	Raadr,				# $v(v(fp))
	Ramadr,				# $v(v(mp))

	Rend:		con byte iota;


Const: adt
{
	val:		big;
	rval:		real;
};

Node: adt
{
	src:		Src;
	op:		int;
	addable:	byte;
	parens:		byte;
	temps:		byte;
	left:		cyclic ref Node;
	right:		cyclic ref Node;
	ty:		cyclic ref Type;
	decl:		cyclic ref Decl;
	c:		ref Const;	# for Oconst
};

	#
	# types visible to limbo
	#
	Tnone,
	Tadt,
	Tadtpick,			# pick case of an adt
	Tarray,
	Tbig,				# 64 bit int
	Tbyte,				# 8 bit unsigned int
	Tchan,
	Treal,
	Tfn,
	Tint,				# 32 bit int
	Tlist,
	Tmodule,
	Tref,
	Tstring,
	Ttuple,

	#
	# internal use types
	#
	Tainit,				# array initializers
	Talt,				# alt channels
	Tany,				# type of nil
	Tarrow,				# unresolved ty->ty types
	Tcase,				# case labels
	Tcasec,				# case string labels
	Tdot,				# unresolved ty.id types
	Terror,
	Tgoto,				# goto labels
	Tid,				# id with unknown type
	Tiface,				# module interface

	Tend:		con iota;

	#
	# marks for various phases of verifing types
	#
	OKbind,				# type decls are bound
	OKverify,			# type looks ok
	OKsized,			# started figuring size
	OKref,				# recorded use of type
	OKclass,			# equivalence class found
	OKcyc,				# checked for cycles
	OKcycsize,			# checked for cycles and size
	OKmodref:			# started checking for a module handle

			con byte 1 << iota;
	OKmask:		con byte 16rff;

	#
	# recursive marks
	#
	TReq,
	TRcom,
	TRcyc:
			con byte 1 << iota;

Type: adt
{
	src:		Src;
	kind:		int;
	ok:		byte;		# set when type is verified
	varargs:	byte;		# if a function, ends with vargs?
	linkall:	byte;		# put all iface fns in external linkage?
	rec:		byte;		# in the middle of recursive type
	pr:		byte;		# in the middle of printing a recursive type
	sbl:		int;		# slot in .sbl adt table
	sig:		int;		# signature for dynamic type check
	size:		int;		# storage required, in bytes
	align:		int;		# alignment in bytes
	decl:		cyclic ref Decl;
	tof:		cyclic ref Type;
	ids:		cyclic ref Decl;
	tags:		cyclic ref Decl;# tagged fields in an adt
	cse:		cyclic ref Case;# case or goto labels
	teq:		cyclic ref Type;# temporary equiv class for equiv checking
	tcom:		cyclic ref Type;# temporary equiv class for compat checking
	eq:		cyclic ref Teq;	# real equiv class
};

#
# type equivalence classes
#
Teq: adt
{
	id:		int;		# for signing
	ty:		cyclic ref Type;# an instance of the class
	eq:		cyclic ref Teq;	# used to link eq sets
};

Tattr: adt
{
	isptr:		int;
	refable:	int;
	conable:	int;
	isbig:		int;
	vis:		int;		# type visible to users
};
