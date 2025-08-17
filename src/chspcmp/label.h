//
//	label.cpp structures
//
#ifndef __label_h
#define __label_h

#include <map>
#include <set>
#include <string>
#include <vector>

#define maxname 256			  // label name max
#define def_maxsymbol 0x10000 // Symbol Table Size (default)
#define def_maxblock 128	  // Symbol Table Block max (default)

#define LAB_TYPE_MARK 0
#define LAB_TYPE_SYSVAL 7
#define LAB_TYPE_INTCMD 8
#define LAB_TYPE_EXTCMD 9
#define LAB_TYPE_IF 11

#define LAB_TYPE_PPVAL 0x110
#define LAB_TYPE_PPMAC 0x111
#define LAB_TYPE_PPMODFUNC 0x112
#define LAB_TYPE_PPDLLFUNC 0x113
#define LAB_TYPE_PPINTMAC 0x114
#define LAB_TYPE_PPEX_INTCMD 0x115
#define LAB_TYPE_PPEX_EXTCMD 0x116
#define LAB_TYPE_PPEX_PRECMD 0x117
#define LAB_TYPE_COMVAR 0x200
#define LAB_TYPE_PP_PREMODFUNC 0x212

#define LAB_DUMPMODE_RESCMD 3
#define LAB_DUMPMODE_DLLCMD 4
#define LAB_DUMPMODE_ALL 15

#define PRM_MASK 0xfff
#define PRM_FLAG_CTYPE 0x1000

#define LAB_INIT_NO 0
#define LAB_INIT_DONE 1

#define LAB_TYPEFIX_NONE 0
#define LAB_TYPEFIX_LABEL 1
#define LAB_TYPEFIX_STR 2
#define LAB_TYPEFIX_DOUBLE 3
#define LAB_TYPEFIX_INT 4
#define LAB_TYPEFIX_STRUCT 5


typedef std::set<std::string> FileNameSet;
typedef std::multimap<std::string, int> LabelMap;
typedef struct LABREL LABREL;

struct LABREL
{
	LABREL *link; // link to next (NULL=end)
	int rel_id;	  // related id
};

typedef struct LABOBJ
{
	int flag;		  // exist flag
	int type;		  // object type
	int opt;		  // option code
	short eternal;	  // eternal flag
	short ref;		  // reference flag
	std::string name; // object name (lower case)
	char *data;		  // data field
	char *data2;	  // data field (opt)
	LABREL *rel;	  // relation id
	short init;		  // initalize flag
	short typefix;	  // force type
	bool skiplablist; // skip generate label list

	std::string def_file;
	int def_line;
} LABOBJ;

//  label manager class
class CLabel
{
public:
	CLabel();
	CLabel( int symmax );
	~CLabel();
	void Reset( void );
	int Regist( const std::string &name, int type, int opt );
	int Regist( const std::string &name, int type, int opt, const std::string &filename, int line );
	void SetEternal( int id );
	int GetEternal( int id );
	void SetOpt( int id, int val );
	void SetFlag( int id, int val );
	void SetData( int id, char *str );
	void SetData2( int id, char *str, int size );
	void SetInitFlag( int id, int val );
	void SetForceType( int id, int val );
	int Search( const std::string &oname );
	int SearchLocal( const std::string &oname, const std::string &loname );
	int GetCount( void );
	int GetFlag( int id );
	int GetType( int id );
	int GetOpt( int id );
	const std::string &GetName( int id );
	char *GetData( int id );
	char *GetData2( int id );
	int GetInitFlag( int id );
	int GetForceType( int id );
	LABOBJ *GetLabel( int id );
	void DumpLabel( char *str );
	int DumpHSPLabelById( int id, char *str, int option );
	void DumpHSPLabel( char *str, int option, int maxsize );
	int RegistList( char **list, const std::string &modname );
	int RegistList2( char **list, const std::string &modname );
	int RegistList3( char **list );
	int GetNumEntry( void )
	{
		return mem_lab.size();
	};
	void AddReference( int id );
	int GetReference( int id );
	void AddRelation( int id, int rel_id );
	void AddRelation( const std::string &name, int rel_id );
	int SearchRelation( int id, int rel_id );
	void SetCaseMode( int flag );
	void SetDefinition( int id, const std::string &filename, int line );
	void SetSkipLabList( int id );

private:
	void StrCase( char *str );

	char *Prt( char *str, const std::string &str2 );
	int HtoI( void );
	char *GetListToken( char *str );
	char *ExpandSymbolBuffer( int size );
	void DisposeSymbolBuffer( void );
	void MakeSymbolBuffer( void );

	char *RegistTable( char *str, int size );

	//	data
	std::vector<std::vector<char>> symbol; // Symbol Table
	std::vector<LABOBJ> mem_lab;		   // Label object

	int symcur;	   // Current Symbol Index
	int maxsymbol; // Max Symbol Size

	char token[64]; // Token for RegistList
	int casemode;	// Case sensitive (0=none/other=ON)

	LabelMap labels;	   // Lookup table
	FileNameSet filenames; // Pool of file names (pointed from def_file)
};

#endif