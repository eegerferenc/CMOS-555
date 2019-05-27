#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>

//This is our design rule set.
//Rules were taken from the transistors in Pearlriver.
//TODO: read this from file in runtime
#define TECH_NAME "scmos"

#define REC_LAMBDA 2 //real size [um] = size [lambda] / REC_LAMBDA

#define R_CSZ 5 //1.5x1.5u contacts (techfile shrinks them by 0.5u each side)
#define R_CSP 1 //1.5u contact spacing (0.5u + 2x0.5u from above)
#define R_CTC 3	//2u contact to channel spacing (0.5u comes from contact shrink) This parameter is ignored on ESD device generation.
#define R_CTE 2	//contact to diff edge spacing 1.5um (0.5u comes from contact shrink)
#define R_PO 3	//1.5u channel end poly overlap
#define R_WELL 6 //well edge to everything, 3um (assumed that the transistors will be butted against each other to form a single n or p island)
#define R_ESD_CTB 1 //ESD device: contact to silicide block spacing 1um (0.5um comes from the contact shrink)
#define R_ESD_SB 0 //ESD device: width of silicide block in drain. If no blocking desired, set to zero.
#define R_ESD_BTG 18 //ESD device: silicide block to channel spacing 9um (if R_ESD_SB==0, then R_ESD_BTG+R_ESD_CTB will be the channel-contact spacing)
#define R_ESD_SBO 3 //ESD device: silicide block extension 1.5um

//layer definitions
#define L_NDIFF "ndiffusion"
#define L_PDIFF "pdiffusion"
#define L_NCONT "ndcontact"
#define L_PCONT "pdcontact"
#define L_NTR "ntransistor"
#define L_PTR "ptransistor"
#define L_MET "metal1"
#define L_POLY "polysilicon"
#define L_NWELL "nwell"
#define L_PWELL "pwell"
#define L_BLOCK NULL //intentionally trigger segfault when used. TODO for magic techfile: give access to silicide block as a separate layer
//#define L_BLOCK "metal2" //for test only

//device definitions (values of "model-name" in gschem, all-caps)
#define NAME_PMOS "PMOS4"
#define NAME_NMOS "NMOS4"
#define NAME_PESD "PESD"
#define NAME_NESD "NESD"

#define NAME_FATTR 'M' //attribute name for finger count (shall be one character)


//This fuction writes a rectangle into the output file. X, Y in lambdas.

void rectangle(FILE* mag, char* layername, int x1, int y1, int x2, int y2){
int xmin;
int xmax;
int ymin;
int ymax;

//magic does not accept "degenerate" rectangles
if(x1==x2){fprintf(stderr, "Librarian:ERROR:rectangle with zero width.\n");abort();}
if(y1==y2){fprintf(stderr, "Librarian:ERROR:rectangle with zero height.\n");abort();}
if(x1>x2){xmin=x2;xmax=x1;}else{xmin=x1;xmax=x2;}
if(y1>y2){ymin=y2;ymax=y1;}else{ymin=y1;ymax=y2;}

fprintf(mag, "<< %s >>\nrect %d %d %d %d\n",layername, xmin, ymin, xmax, ymax); 
}

//This function generates the contacts (w in lambdas)
//note: w must be larger than 5 lambda (2.5um)
//pol: 0 for pmos, otherwise nmos
void contact(FILE* mag, int w, int pol, int offset){
int vs;
int done;
char* layername_contact;
char* layername_diff;
done=0;
vs=0;

//determine polarity
if(pol){
layername_contact=L_NCONT;
layername_diff=L_NDIFF;
}else{
layername_contact=L_PCONT;
layername_diff=L_PDIFF;
}

//place initial spacer
rectangle(mag, layername_diff, offset, vs, (offset+R_CSZ), (vs+R_CTE));
rectangle(mag, L_MET, offset, vs, (offset+R_CSZ), (vs+R_CTE));
vs=vs+R_CTE;

//place contacts
while(!done){	
if((w-vs)>=(R_CSZ+R_CTE)){	//Do we have more space for a contact?
rectangle(mag, layername_contact, offset, vs, (offset+R_CSZ), (vs+R_CSZ));	//Then place one
vs=vs+R_CSZ;}
if((w-vs)>=(R_CSZ+R_CTE+R_CSP)){	//Would a next contact follow?
rectangle(mag, layername_diff, offset, vs, (offset+R_CSZ), (vs+R_CSP));	//Then place a spacer
rectangle(mag, L_MET, offset, vs, (offset+R_CSZ), (vs+R_CSP));
vs=vs+R_CSP;}
else{done=1;}	//No more space for a contact, done
}

//Place filler
if((w-vs)>=0) rectangle(mag, layername_diff, offset, vs, (offset+R_CSZ), w);
if((w-vs)>=0) rectangle(mag, L_MET, offset, vs, (offset+R_CSZ), w);
}



//This function generates a transistor into an already open file
//note: w must be larger than (2*R_CTE)+R_CSZ lambda (we don't support dogbone layout yet) TODO:support it
//pol: 0 for pmos, otherwise nmos
//f: number of fingers
//esd: if nonzero, we will generate an ESD protection transistor with lenghtened drain
void transistor(FILE* mag, int w, int l, int f, int pol, int esd){
int i;
int offset;
char* layername_contact;
char* layername_diff;
char* layername_transistor;
char* layername_well;
offset=0;

//determine polarity
if(pol){
layername_contact=L_NCONT;
layername_diff=L_NDIFF;
layername_transistor=L_NTR;
layername_well=L_PWELL;
}else{
layername_contact=L_PCONT;
layername_diff=L_PDIFF;
layername_transistor=L_PTR;
layername_well=L_NWELL;
}

//generate initial spacer
rectangle(mag, layername_diff, offset, 0, (offset+R_CTE), w);
offset=offset+R_CTE;

//generate the fingers
for(i=1;i<=f;i++){

	//generate contact
	rectangle(mag, L_MET, (offset-R_CTE), 0, offset, w);
	contact(mag, w, pol, offset);
	offset=offset+R_CSZ;
	rectangle(mag, L_MET, offset, 0, (offset+R_CTE), w);

	//generate channel end
	if(!esd){	//normal transistor
		rectangle(mag, layername_diff, offset, 0, (offset+R_CTC), w);
		offset=offset+R_CTC;
	}else{	//ESD device
		if((i%2)==0){//drain or source?
			rectangle(mag, layername_diff, offset, 0, (offset+R_ESD_CTB), w);
			offset=offset+R_ESD_CTB;

			if(R_ESD_SB){ //only do this if silicide block is requested
				rectangle(mag, layername_diff, offset, 0, (offset+R_ESD_SB), w);
				rectangle(mag, L_BLOCK, offset, (0-R_ESD_SBO), (offset+R_ESD_SB), (w+R_ESD_SBO));
				offset=offset+R_ESD_SB;}

			rectangle(mag, layername_diff, offset, 0, (offset+R_ESD_BTG), w);
			offset=offset+R_ESD_BTG;

		}else{//ESD transistor, but source
			rectangle(mag, layername_diff, offset, 0, (offset+R_CTC), w);
			offset=offset+R_CTC;
		}//end if((i%2)==0){//drain or source?
	}//end if(!esd)

//generate actual transistor
rectangle(mag, L_POLY, offset, (0-R_PO), (offset+l), 0);
rectangle(mag, layername_transistor, offset, 0, (offset+l), w);
rectangle(mag, L_POLY, offset, w, (offset+l), (w+R_PO));
offset=offset+l;

//generate channel end
if(!esd){	//normal transistor
rectangle(mag, layername_diff, offset, 0, (offset+R_CTC), w);
offset=offset+R_CTC;
}else{	//ESD device
if((i%2)==1){//drain or source?

rectangle(mag, layername_diff, offset, 0, (offset+R_ESD_BTG), w);
offset=offset+R_ESD_BTG;


if(R_ESD_SB){ //only do this if silicide block is requested
rectangle(mag, layername_diff, offset, 0, (offset+R_ESD_SB), w);
rectangle(mag, L_BLOCK, offset, (0-R_ESD_SBO), (offset+R_ESD_SB), (w+R_ESD_SBO));
offset=offset+R_ESD_SB;}

rectangle(mag, layername_diff, offset, 0, (offset+R_ESD_CTB), w);
offset=offset+R_ESD_CTB;

}else{//ESD transistor, but source
rectangle(mag, layername_diff, offset, 0, (offset+R_CTC), w);
offset=offset+R_CTC;}
}
}


//generate terminal contact
	rectangle(mag, L_MET, (offset-R_CTE), 0, offset, w);
	contact(mag, w, pol, offset);
	offset=offset+R_CSZ;
	rectangle(mag, layername_diff, offset, 0, (offset+R_CTE), w);
	rectangle(mag, L_MET, offset, 0, (offset+R_CTE), w);
	offset=offset+R_CTE;

//generate well
rectangle(mag, layername_well, (0-R_WELL), (0-R_PO-R_WELL), (offset+R_WELL), (w+R_PO+R_WELL));

//this will force a DRC of cell generated outside magic
rectangle(mag, "checkpaint", ((0-R_WELL)-1), ((0-R_PO-R_WELL)-1), (offset+R_WELL+1), (w+R_PO+R_WELL+1));
}

//This function will create the library element (creates the file and its contents.)
//w,l in microns, f,pol,esd as usual.
void create_transistor(double w, double l, int f, int pol, int esd){
char* outfile;
int i;
FILE* mag;

//preparing the output filename
outfile=malloc(100);
if(!outfile){fprintf(stderr, "Librarian:ERROR:cannot malloc.\n");abort();}
fprintf(stderr, "Librarian:creating ");
if(esd){
	if(pol){
	fprintf(stderr, "N-channel ");
	sprintf(outfile, "LIB_NESD_W%g_L%g_F%d", w, l, f);
	}else{
	fprintf(stderr, "P-channel ");
	sprintf(outfile, "LIB_PESD_W%g_L%g_F%d", w, l, f);}
	fprintf(stderr, "ESD device ");
}else{
	if(pol){
	fprintf(stderr, "N");
	sprintf(outfile, "LIB_NMOS_W%g_L%g_F%d", w, l, f);
	}else{
	fprintf(stderr, "P");
	sprintf(outfile, "LIB_PMOS_W%g_L%g_F%d", w, l, f);}
	fprintf(stderr, "MOS device ");
}
fprintf(stderr, "with parameters W= %g um, L= %g um, Fingers= %d ...\n", w, l, f);
i=0;
while(outfile[i]){if(outfile[i]=='.')outfile[i]='P';i++;}
strcat(outfile, ".mag");

if(((int)(w*(double)REC_LAMBDA))<((2*R_CTE)+R_CSZ)){
fprintf(stderr, "Librarian:WARNING:creation of %s skipped: w must be larger than (2*R_CTE)+R_CSZ lambda (we don't support dogbone layout yet)\n", outfile);
return;}

// open output file
//TODO: skip the whole thing if the file already exists

mag=fopen(outfile, "w");
if(!mag){
fprintf(stderr, "Librarian:ERROR:cannot open output file %s: %s\n", outfile, strerror(errno));
abort();
}
fprintf(stderr, "Librarian:output file %s opened.\n", outfile);

// write out header

fprintf(mag, "magic\n");
fprintf(mag, "tech %s\n", TECH_NAME);
fprintf(mag, "timestamp %ld\n", time(NULL));

//generate the device

transistor(mag, (int)(w*(double)REC_LAMBDA), (int)(l*(double)REC_LAMBDA), f, pol, esd);

//write out footer and close output file

fprintf(mag, "<< end >>\n");
if(fclose(mag)){
fprintf(stderr, "Librarian:ERROR:cannot close output file %s: %s\n", outfile, strerror(errno));
abort();
}
fprintf(stderr, "Librarian:output file %s closed.\n", outfile);


free(outfile);
}

void print_usage(void){
fprintf(stderr, "Librarian: static layout library generator for Libresilicon/magic.\n\nUsage: librarian <netlist-filename>\n\nInput: a SPICE netlist\nOutput: a group of .mag files containing MOSFET devices generated according to the parameters found in the SPICE netlist, deposited into the current working directory.\n\nTHIS SOFTWARE COMES WITH NO WARRANTIES, INCLUDING THE IMPLIED WARRANTY FOR MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.\n\nFor further information, refer to the source code.\n\n");
}

//struct to form chained list of transistor types to be generated
typedef struct tr_list_str tr_list_t;
typedef struct tr_list_str {
double w; //W
double l; //L
int f; //Number of fingers
int pol; //0 for pmos, otherwise nmos
int esd; //nonzero for ESD device, 0 for normal MOSFET
tr_list_t* next;
} tr_list_t;

//This function parses a spice line into a tr_list_t struct.

tr_list_t* parse_spice(char* line){
tr_list_t* ret;
int wlf;
int i;
char* buf;
ret=malloc(sizeof(tr_list_t));if(!ret){fprintf(stderr, "Librarian:ERROR:cannot malloc.\n");abort();}

//refdes
buf=strtok(line, " ");
fprintf(stderr, "Librarian:found device with refdes %s, listing as ", buf);

//connections
strtok(NULL, " ");
strtok(NULL, " ");
strtok(NULL, " ");
strtok(NULL, " ");

//device type
buf=strtok(NULL, " ");
if(!strcmp(buf,NAME_NMOS)){fprintf(stderr, "NMOS device");ret->pol=1;ret->esd=0;}
else if(!strcmp(buf,NAME_PMOS)){fprintf(stderr, "PMOS device");ret->pol=0;ret->esd=0;}
else if(!strcmp(buf,NAME_NESD)){fprintf(stderr, "N-channel ESD device");ret->pol=1;ret->esd=1;}
else if(!strcmp(buf,NAME_PESD)){fprintf(stderr, "P-channel ESD device");ret->pol=0;ret->esd=1;}
else {fprintf(stderr, "Librarian:ERROR:unrecognized device %s.\n", buf);abort();}

//process everything else
wlf=0;
while(1){
buf=strtok(NULL, " ");
if(!buf)break;

if((buf[0]=='W')&&(buf[1]=='=')){
	i=strlen(buf);
	if(buf[i-1]!='U'){fprintf(stderr, " ...\nLibrarian:ERROR:parameter W shall end in U.\n");abort();} //TODO:handle this better (e.g. replace prefix with exponent)
	buf[i-1]=0;
	buf=buf+2;
	if(sscanf(buf, "%lf", &ret->w)!=1){fprintf(stderr, " ...\nLibrarian:ERROR:parameter W cannot be parsed.\n");abort();} //DANGER: sscanf uses the locale (. or ,)!
	fprintf(stderr, ", W= %g um", ret->w);
	wlf=wlf+4;
	}

if((buf[0]=='L')&&(buf[1]=='=')){
	i=strlen(buf);
	if(buf[i-1]!='U'){fprintf(stderr, " ...\nLibrarian:ERROR:parameter L shall end in U.\n");abort();} //TODO:handle this better (e.g. replace prefix with exponent)
	buf[i-1]=0;
	buf=buf+2;
	if(sscanf(buf, "%lf", &ret->l)!=1){fprintf(stderr, " ...\nLibrarian:ERROR:parameter L cannot be parsed.\n");abort();} //DANGER: sscanf uses the locale (. or ,)!
	fprintf(stderr, ", L= %g um", ret->l);
	wlf=wlf+2;
	}

if((buf[0]==NAME_FATTR)&&(buf[1]=='=')){
	buf=buf+2;
	if(sscanf(buf, "%d", &ret->f)!=1){fprintf(stderr, " ...\nLibrarian:ERROR:parameter F cannot be parsed.\n");abort();}
	fprintf(stderr, ", F= %d", ret->f);
	wlf=wlf+1;
	}

}
fprintf(stderr, "\n");
if(wlf==6){
ret->f=1;
wlf=wlf+1;
fprintf(stderr, "Librarian:WARNING:implicitly assuming single-finger transistor\n");}

if(wlf!=7){fprintf(stderr, "Librarian:ERROR:incomplete transistor definition.\n");abort();}

return ret;
}

//Main entry point

int main(int argc, char** argv){
FILE* netlist;
char* netlist_line;
int i;

tr_list_t* curr;
tr_list_t* tr_list;

tr_list=NULL;

fprintf(stderr, "Librarian:librarian invoked.\n");

netlist_line=malloc(1000);
if(!netlist_line){fprintf(stderr, "Librarian:ERROR:cannot malloc.\n");abort();}

//handle help stuff and validate input filename

if(argc!=2){print_usage();return 1;}
netlist=fopen(argv[1], "r");
if(!netlist){
fprintf(stderr, "Librarian:ERROR:cannot open netlist file %s: %s\n", argv[1], strerror(errno));
abort();
}
fprintf(stderr, "Librarian:netlist file %s opened.\n", argv[1]);

//Parse input file line-by-line and extract MOSFET definitions

while(fgets(netlist_line,1000,netlist)!=NULL){
i=strlen(netlist_line);
netlist_line[i-1]=0;	//removing the trailing newline
for(i=i-2;i>-1;i--)netlist_line[i]=toupper(netlist_line[i]);	//making it case-insensitive

if(netlist_line[0]!='M')continue;	//Not a MOSFET, don't care

//Add the MOSFET to the list

curr=parse_spice(netlist_line);
curr->next=tr_list;
tr_list=curr;
}

if(fclose(netlist)){
fprintf(stderr, "Librarian:ERROR:cannot close netlist file %s: %s\n", argv[1], strerror(errno));
abort();
}
fprintf(stderr, "Librarian:netlist file %s closed.\n", argv[1]);

//doing the cell generation
curr=tr_list;
while(curr){
create_transistor(curr->w, curr->l, curr->f, curr->pol, curr->esd);
curr=curr->next;
}

fprintf(stderr, "Librarian:exiting.\n");
return 0;
}




