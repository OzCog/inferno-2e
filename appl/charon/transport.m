Transport: module
{
	HTTPPATH: con "/dis/charon/http.dis";
	FTPPATH: con "/dis/charon/ftp.dis";
	FILEPATH: con "/dis/charon/file.dis";

	Tmax: con Url->FILE+1;

	init:		fn(cu: CharonUtils);
	connect:	fn(nc: ref CharonUtils->Netconn, bs: ref CharonUtils->ByteSource);
	writereq:	fn(nc: ref CharonUtils->Netconn, bs: ref CharonUtils->ByteSource);
	gethdr:	fn(nc: ref CharonUtils->Netconn, bs: ref CharonUtils->ByteSource);
	getdata:	fn(nc: ref CharonUtils->Netconn, bs: ref CharonUtils->ByteSource);
	defaultport:	fn(scheme: int) : int;
};
