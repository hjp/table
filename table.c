char	table_c_rcs_id [] =
	"$Id: table.c,v 1.1 1998-08-10 23:43:10 hjp Exp $";
/*
 * Tabulate files.
 *
 *	$Log: table.c,v $
 *	Revision 1.1  1998-08-10 23:43:10  hjp
 *	Initial revision
 *
 * Revision 1.7  1994/07/06  12:40:19  hjp
 * fixed bug with delimiters > 0x80
 *
 * Revision 1.6  1994/05/22  19:47:26  hjp
 * doesn't hang any more if a field is longer than max. line width
 *
 * Revision 1.5  1993/11/21  17:26:40  hjp
 * avoid trying to resort 0 output fields. This resulted in an error message
 * from emalloc on empty input files.
 * check for write errors on stdout
 *
 * Revision 1.4  1993/06/02  20:34:25  hjp
 * removed limit on number of lines
 *
 * Revision 1.3  1993/06/01  23:14:14  hjp
 * added -w option to specify maximum output line with. Longer lines are folded.
 * added -l option to specify line separator
 *
 * Revision 1.2  1992/12/13  10:33:14  hjp
 * merged with DOS version 1.2 (1.1.1.2)
 *
 * Revision 1.2  1992/10/03  23:25:20  hjp
 * reduced memory consumption by using large blocks of memory.
 * removed padding of last field of line
 * removed empty last line
 *
 */
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if MALLOC_TRACE
#include "malloctrace.h"
#define emalloc(size) malloc_trace(size)
#define erealloc(ptr, size) realloc_trace(ptr, size)
#else
#include <ant/alloc.h>
#endif

#define BUFSIZE		0x4000U
#define FIELDSIZE	0x1000U
#define LINESIZE        0x1000U
#define WIDTHSIZE	16

#if defined (__STDC__) || defined (__TURBOC__)
#	define _(x) x
#else
#	define _(x) ()
#endif

void usage _((void));
void table _((FILE *fp));
int main _((int, char **));

typedef struct {
	unsigned int	width;
	unsigned int	nr;
}	orderT;

char * cmnd;

void usage ()
{
	fprintf (stderr, "Usage: %s [-d delimiter] [-2] [-w width] [-l linedel] file ...\n",
		 cmnd);
	exit (1);
}

static int	delimiter = '\t';
static int	twopass;
static int	width = INT_MAX;
static char	*linedel = NULL;


int ordercmp (const void *a, const void *b)
/* comparison function to sort field of |orderT|s by decreasing |width| */
{
	const orderT	*aa = a, *bb = b;
	return bb->width - aa->width;
}


int resort (		/* Returns: Number of fields per output line */
	orderT	**orderp
			/* InOut:	fieldwidths and numbers	*/,
	size_t	*nelp	/* InOut:	number of elements in order */
)
/* resort fields, so that a maximum number of fields fits onto one 
 * output line. This is done by sorting by size, then printing in 
 * columns
 *
 * Uses:	width ... maximum width of an output line
 */
{
	int	plines;	/* nr of physical output lines */
	orderT	*order = *orderp;
	orderT	*neworder;
	size_t	nel = *nelp;
	size_t	onel = *nelp;
	int	totalwidth;
	int	field;
	int	i;

	qsort (order, nel, sizeof (*order), ordercmp);

	plines = 1;

	do {
		plines ++;
		totalwidth = 0;	/* last field has delimiter, too */
		for (field = 0; field < nel; field += plines) {
			totalwidth += order [field].width + 1;
		}
	} while (totalwidth > width && plines < nel);
	nel = (nel + plines - 1) / plines * plines;

	neworder = emalloc (nel * sizeof (*neworder));
	for (i = 0; i < nel; i ++) {
		field = i * plines % nel + i * plines / nel;
		neworder [i].nr = field >= onel ? -1 : order [field].nr;
		neworder [i].width = order [i * plines % nel].width; /* max of column */
	}
	free (order);
	*orderp = neworder;
	*nelp = nel;
	return nel / plines;
}

void table (fp)
	FILE	* fp;
/* Tabulate the file read from |fp|. If the resulting lines are longer 
 * than |width|, they are broken into several physical lines.
 * In this case, |delimiter| serves as a continuation character.
 *
 * Uses: delimiter	... character delimiting fields
 *	 width		... maximum output line width
 */
{
	/* Memory organization is as follows: Text and pointers to 
	 * fields are kept in large buffers of fixed size. The text
	 * of a single field and the field pointers of a single line
	 * cannot cross a buffer boundary. If that happens, the whole 
	 * field or line is copied to a new buffer (This limits the
	 * number of characters in a field to |BUFSIZE| and the number
	 * of fields per line to |FIELDSIZE|. Similarily all the pointers
	 * to lines are kept in a single buffer. To avoid another 
	 * level of indirection (triple pointers are bad enough), this 
	 * buffer can be resized, though. DOS systems are still limited
	 * to ó 16384 lines.
	 */

	int	c;
	int	start = 1;
        char	*buffer,
        	*curchar,
                *fieldstart;		/* start of current field */
        char	*lastnonblank = NULL;

        char	**fields;
	char	**curfield;
        char	**linestart;		/* start of current line */

        char	***lines;
	char	***curline;

        size_t	*widths;
        size_t	nr_widths = WIDTHSIZE;
	int	i,
		field = 0;
	int	totalwidth;		/* total width of an output
					 * line
					 */
	int	fieldsperline;
	orderT	*order;
	size_t	linesize = LINESIZE;



        buffer = emalloc (BUFSIZE);
        fields = emalloc (FIELDSIZE * sizeof (*fields));
        lines  = emalloc (linesize * sizeof (*lines));
        widths = emalloc (WIDTHSIZE * sizeof (*widths));
        for (i = 0; i < nr_widths; i ++) widths [i] = 0;

        curchar = fieldstart = buffer;
        curfield = linestart = fields;
        curline = lines;


	while ((c = getc (fp)) != EOF) {
#ifdef DEBUG_TABLE
		fprintf (stderr, "c = '%c'\n", c);
#endif

                if (curchar >= buffer + BUFSIZE) {
                        buffer = emalloc (BUFSIZE);
                        memcpy (buffer, fieldstart, curchar - fieldstart);
                        curchar = buffer + (curchar - fieldstart);
                        if (lastnonblank) {
				lastnonblank = buffer + (lastnonblank - fieldstart);
			}
			fieldstart = buffer;
                }

		if (c == delimiter || c == '\n') {
			/* terminate field	*/
                        *curfield = fieldstart;
                        if (lastnonblank) curchar = lastnonblank + 1;
                        *curchar++ = '\0';

			/* adjust width of field if necessary	*/
			if (field >= nr_widths) {
                        	widths = erealloc (widths, sizeof (*widths) * nr_widths * 2);
                                for (i = nr_widths; i < nr_widths * 2; i ++) {
                                	widths [i] = 0;
                                }
                                widths [field] = curchar - fieldstart;
                                nr_widths *= 2;
			} else if (curchar - fieldstart - 1 > widths [field]) {
				widths [field] = curchar - fieldstart - 1;
			}
			/* prepare next field	*/
			field ++;
			curfield ++;
	                if (curfield >= fields + FIELDSIZE) {
                                fields = emalloc (FIELDSIZE * sizeof (*fields));
	                        memcpy (fields, linestart,
					(char *)curfield - (char *)linestart);
                        	curfield = fields + (curfield - linestart);
                                linestart = fields;
                        }
                        fieldstart = curchar;
                        lastnonblank = NULL;
			start = 1;
                        if (c == '\n') {
                        	*curfield ++ = NULL;
	                        if (curfield >= fields + FIELDSIZE) {
                                	fields = emalloc (FIELDSIZE * sizeof (*fields));
                                        curfield = fields;
                                }
                        	*curline ++ = linestart;
                                if (curline > lines + linesize) {
                                	char ***newlines;
                                	assert (linesize * (2 * sizeof (*lines)) / (2 * sizeof (*lines)) == linesize);
                                	linesize *= 2;
                                	newlines = erealloc (lines, linesize * sizeof (*lines));
                                	curline = curline - lines + newlines;
                                	lines = newlines;
                                }
                                linestart = curfield;
                                field = 0;
                        }
		} else if (start && isspace (c)) {
			/* do nothing	*/
		} else {

                	*curchar = c;
                        if (! isspace (c)) {
				lastnonblank = curchar;
				start = 0;
                        }
                        curchar ++;
		}
	}
        *curline = NULL;

	/* check if all fields fit into one line, if not, 
	 * break them up to several physical lines. Sort by size to
	 * maximize number of fields per output line.
	 */
	while (nr_widths > 0 && widths[nr_widths - 1] == 0) nr_widths --;

	if (nr_widths == 0) return; /* nothing to do */

	order = emalloc (nr_widths * sizeof (*order));

	totalwidth = -1;
	for (i = 0; i < nr_widths; i ++) {
#ifdef DEBUG_TABLE
                fprintf(stderr, "%d ", widths [i]);
#endif
		order [i].width = widths [i];
		order [i].nr = i;
		totalwidth += widths [i] + 1;
	}

	if (totalwidth > width) {
		fieldsperline = resort (&order, &nr_widths);
	} else {
		fieldsperline = nr_widths;
	}

	/* print file	
	 */
	for (curline = lines; *curline; curline ++) {
	     	for (curfield = *curline, field = 0;
		     *curfield;
		     curfield ++, field++);
	     	for (i = 0; i < nr_widths; i += fieldsperline) {
	     		int	j;
	     		for (j = 0; j < fieldsperline; j ++) {
				if (i + j < nr_widths - 1) {
					printf ("%*s",
						(int) -order [i + j].width,
						order [i + j].nr < field
						    ? (*curline) [order [i + j].nr]
						    : ""
					       );
					putchar (delimiter);
				} else {
					printf ("%s",
						order [i + j].nr < field
						    ? (*curline) [order [i + j].nr]
						    : ""
					       );
				}
			}
			putchar ('\n');
		}
		if (linedel) fputs (linedel, stdout);
		if (ferror (stdout)) {
			fprintf (stderr, "%s: write error on stdout: %s\n",
				 cmnd, strerror (errno));
			exit (1);
		}
	}
	if (fflush (stdout) == EOF) {
		fprintf (stderr, "%s: write error on stdout: %s\n",
			 cmnd, strerror (errno));
		exit (1);
	}
}

#ifdef TESTMEM
#include <alloc.h>
long		stillfree;
int		verbose;
struct heapinfo	hi;
#endif

	int
main (argc, argv)
	int	argc;
	char	**argv;
{
	FILE	*fp = 0;
	
	cmnd = argv [0];
	
	while (*++ argv) {
		if (**argv == '-') {
			if ((*argv)[1]) {
				while (*++* argv) {
					switch (** argv) {
						char * p;
					case 'd':
						if (! *++* argv) ++argv;
						if (!* argv || !** argv) usage (); 
						if (isdigit ((unsigned char)**argv)) {
							delimiter = strtol (* argv, &p, 0);
							if (* p) usage ();
						} else {
							delimiter = (unsigned char) ** argv;
						}
						goto nextargv;
						break;
					case '2':
						twopass = 1;
						break;
					case 'w':
						if (! *++* argv) ++argv;
						if (!* argv || !** argv) usage (); 
						width = strtol (* argv, &p, 0);
						if (* p || width < 2) usage ();
						goto nextargv;
						break;
					case 'l':
						if (! *++* argv) ++argv;
						if (!* argv || !** argv) usage (); 
						linedel = *argv;
						goto nextargv;
						break;
#ifdef TESTMEM
                                        case 'v':
                                        	verbose = 1;
                                                stillfree = coreleft ();
                                                break;
#endif
					default:
						usage ();
						break;
					}
				}
			} else {
				table (stdin);
                                fp = stdin;
			}
		} else {
			if ((fp = fopen (* argv, "r")) != NULL) {
				table (fp);
				fclose (fp);
			} else {
				fprintf (stderr, "%s: cannot open %s; %s",
					 cmnd, * argv, strerror (errno));
				exit (1);	 
			}
		}
	nextargv:	
		;
	}
        if (! fp) {
        	table (stdin);
        }
#ifdef TESTMEM
	if (verbose) {
                hi.ptr = NULL;
                fprintf(stderr, "Address     Size   Status\n" );
                fprintf(stderr, "-------     ----   ------\n" );
                while( heapwalk( &hi ) == _HEAPOK )
	                fprintf(stderr, "%p %7u    %s\n", hi.ptr, (unsigned) hi.size, hi.in_use ? "used" : "free" );
		fprintf (stderr, "memory used: %ld bytes\n", stillfree - coreleft ());
        }
#endif
	return 0;
}	
