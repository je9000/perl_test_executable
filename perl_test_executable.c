/* perl_test_executable version 0.9, copyright John Eaglesham */

#include <err.h>
#include <unistd.h>
#include <sysexits.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <EXTERN.h>               /* from the Perl distribution */
#include <perl.h>                 /* from the Perl distribution */

/* In case there are problems running the tests (eg, file not found), we need
   to catch and throw them here. */
static const char *perl_do_code = 
    "sub { $_ = shift; my $eval_r = eval( do { open(my $fh, '<', '%s') || die $!; local $/ = undef; my $code = <$fh>; close( $fh ); $code .= \"\nreturn '_!_je_%lu'\"; $code; } ); if ( !defined $eval_r ) { warn \"Got unexpected value from tests, possible causes: bad tests, \"; if ( $@ ) { die \"$@\n\"; } else { die \"$!\n\"; } } elsif ( $eval_r ne '_!_je_%lu' ) { die \"Got unexpected return value from tests!\n\"; } }";

static const char *padwalk_main = "use PadWalker; PadWalker::peek_my(0);";
static const char *M_devel_cover = "-MDevel::Cover";

EXTERN_C void xs_init (pTHX);
EXTERN_C void boot_DynaLoader (pTHX_ CV* cv);
EXTERN_C void
xs_init(pTHX)
{
    char *file = __FILE__;
    dXSUB_SYS;

    newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
}

int check_script( char *testfile ) {
    PerlInterpreter *my_perl;
    int r = 0;
    char *my_argv[] = { "", "-c", testfile };

    my_perl = perl_alloc();
    if ( my_perl == NULL ) {
        errx( 1, "perl_alloc failed" );
    }
    perl_construct( my_perl );
    r = perl_parse( my_perl, xs_init, 2, my_argv, NULL );
    if ( !r ) {
        perl_run( my_perl );
    }
    perl_destruct( my_perl );
    perl_free( my_perl );
    PERL_SYS_TERM();

    exit( r );
}

int exec_self_for_check( char *testfile ) {
    pid_t child;
    int status;

    child = fork();
    if ( child == -1 ) err( 1, "Failed to fork" );

    /* Child fork. */
    if ( !child ) {
        /* Close STDIN/OUT/ERR and re-open them to /dev/null. Since those
           three are the first three handles, and the only ones open, the next
           thee files we open will have their file descriptors set
           automatically.  */
        close( STDIN_FILENO );
        close( STDOUT_FILENO );
        close( STDERR_FILENO );
        open( "/dev/null", O_RDWR );
        open( "/dev/null", O_RDWR );
        open( "/dev/null", O_RDWR );
        /* check_script will exit with the proper status. */
        check_script( testfile );
    }

    waitpid( child, &status, 0 );
    if ( WIFEXITED( status ) ) {
        return WEXITSTATUS(status);
    }
    /* If it didn't exit normally, return an error code. */
    return 1;
}

/* This stuff is broken out into a separate function because of the way
   the dSP macro works: it wants my_perl to be defined before it is
   called. So, we appease it by breaking this step out into its own
   function. */
void run_test_sv( PerlInterpreter *my_perl, SV *do_sub, SV *original_variables ) {
    dSP;
    ENTER;
    SAVETMPS;
    PUSHMARK(SP);
    XPUSHs(original_variables);
    PUTBACK;
    call_sv( do_sub, G_VOID );
    FREETMPS;
    LEAVE;
}

/* Note, in the context of this function (and the Perl API in general, "eval"
   does not mean the same thing as it does inside a perl script. "eval" here
   does parse and run perl code, but the same "exception" handling isn't done,
   so we do that ourselves via the eval_pv result code. Of course, don't
   confuse that with the eval variable, which we do actually eval... */
int init_perl_test_framework( char *perlfile, char *testfile, char *module, char *eval, char **env, int do_cover, char *pass_to_cover ) {
    PerlInterpreter *my_perl;
    char *eval_me = NULL;
    char *new_module = NULL;
    char *cover_flags = NULL;
    /* Pre-allocate to the maximum number we might possibly need. */
    char *my_argv[4];
    int argc = 0;
    int r;
    unsigned long int rand_for_return;
    SV *do_sub;
    SV *original_variables;

    /* It's this, or goto. Don't blame me, I like goto. */
    int clean_up(char *error_message) {
        int retval;
        if ( eval_me != NULL ) free( eval_me );
        if ( new_module != NULL ) free( new_module );
        if ( cover_flags != NULL ) free( cover_flags );
        retval = perl_destruct( my_perl );
        perl_free( my_perl );
        PERL_SYS_TERM();
        if ( error_message ) errx( 1, error_message );
        /* Not reached if error_message is set! */
        return retval;
    }

    r = open( "/dev/urandom", O_RDONLY );
    if ( r < 0 ) err( 1, "Failed to open /dev/urandom for reading" );
    if ( read( r, &rand_for_return, sizeof( rand_for_return ) ) < sizeof( rand_for_return ) ) {
        close( r );
        errx( 1, "Failed to read random data!" );
    }
    close( r );
    /* Calculate the length of the string rand_for_return converts into
       without a trailing null, because we use the trailing null from
       eval_me. 

       The cast here won't overflow, because longs can't hold enough data
       to convert into 2**31 digits. */
    r = (int) floor( log( rand_for_return ) / log( 10 ) ) + 1;

    /* Add the length of the file we open, the length of the template itself,
       and the length of the two random numbers we insert.

       perl_do_code contains 8 characters worth of format codes, so subtract
       that many characters.

       Add one character for the trailing NULL.
       */
    r = strlen( testfile ) + strlen( perl_do_code ) + r + r - 8 + 1;
    eval_me = ( char* ) malloc( r );
    if ( eval_me == NULL ) {
        err( 1, "malloc failed" );
    }
    eval_me[0] = ( char ) NULL;
    snprintf( eval_me, r, perl_do_code, testfile, rand_for_return, rand_for_return );

    /* Should we pass in command line arguments to the target script?
       PERL_SYS_INIT3(&argc, &argv, &env); */
    /* Cast to void here to avoid warning message about unused return value.
       I don't even know what the return value here is, this is all Perl
       magic. Note, we only need this cast with GCC < 3, as GCC 3 doesn't seem
       to care, and GCC 4 warns with the cast, but not without. */
#if __GNUC__ < 3
    (void) PERL_SYS_INIT3( NULL, NULL, &env );
#else
    PERL_SYS_INIT3( NULL, NULL, &env );
#endif

    my_perl = perl_alloc();
    if ( my_perl == NULL ) {
        free( eval_me );
        errx( 1, "perl_alloc failed" );
    }

    my_argv[0] = "";
    if ( do_cover ) {
        /* Similar to above, it's the length of the template string plus the
           length of the trailing NULL character.

           If we're passing in arguments from the user, append the length of
           those, plus one for the separating equals sign, too. */
        r = strlen( M_devel_cover ) + 1;
        if ( pass_to_cover ) r += strlen( pass_to_cover ) + 1;
        cover_flags = ( char* ) malloc( r );
        if ( cover_flags == NULL ) {
            free( eval_me );
            errx( 1, "Failed to allocate string" );
        }
        snprintf( cover_flags, r, M_devel_cover );
        if ( pass_to_cover ) {
            strncat( cover_flags, "=", r );
            strncat( cover_flags, pass_to_cover, r );
        }
        my_argv[1] = cover_flags;
        argc++;
    }

    if ( module ) {
        r = strlen( module ) + 3;
        new_module = (char *) malloc( r );
        if ( !new_module ) {
            free( eval_me );
            if ( cover_flags ) free( cover_flags );
            errx( 1, "Failed to allocate string" );
        }
        snprintf( new_module, r, "-M%s", module );
        argc++;
        my_argv[argc] = new_module;
    }

    argc++;
    my_argv[argc] = perlfile;

    PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
    perl_construct( my_perl );
    r = perl_parse( my_perl, xs_init, 2, my_argv, env );
    if ( r ) {
        clean_up( "perl_parse failed" ); 
        /* Not reached! */
        return 1;
    }

    /* Store a reference to all global variables, thanks to PadWalker. */
    original_variables = eval_pv( padwalk_main, TRUE );
    if ( !SvOK( original_variables ) ) {
        clean_up( "Failed to store original variables!" );
        /* Not reached! */
        return 1;
    }

    if ( eval ) {
        eval_sv( newSVpv( eval, 0 ), G_VOID );
        if ( SvTRUE( ERRSV ) ) {
            fputs( SvPV_nolen( ERRSV ), stderr );
            clean_up( "eval failed" );
            /* Not reached! */
            return 1;
        }
    }

    /* Now, create an anonymous sub that reads its parameter (the globals) and
       calls our tests. */
    do_sub = eval_pv( eval_me, TRUE );
    if ( !SvOK( do_sub ) ) {
        clean_up( "Failed to create 'do sub'!" );
        /* Not reached! */
        return 1;
    }

    /* Now, actually run the do_sub and pass it the parameter of the original
       script's global variables. */
    run_test_sv( my_perl, do_sub, original_variables );
    return clean_up( NULL );
}

void usage(void) {
    printf( "usage: perl_test_executable <options>\n" );
    printf( " -p\tPerl script file to test. Required without \"-c\".\n" );
    printf( " -t\tPerl script file containing tests. Required without \"-c\".\n" );
    printf( " -e\tCode to evaluate before loading tests (like perl -e).\n" );
    printf( " -c\tCompile script only (like perl -c), unless using \"-t\" and \"-p\".\n" );
    printf( " -C\tRun with Devel::Cover.\n" );
    printf( " -O\tAdditional options to pass to Devel::Cover (in comma format).\n" );
    printf( " -M\tModule to load with the script to be tested.\n" );
    printf( " -h\tThis message.\n" );
    exit(EX_USAGE);
}

int main(int argc, char **argv, char **env) {
    char opt;
    char *perlfile = NULL;
    char *testfile = NULL;
    char *compile_file = NULL;
    char *eval = NULL;
    char *module = NULL;
    char *cover_args = NULL;
    int do_cover = 0;

    while ( ( opt = getopt(argc, argv, "p:t:e:c:M:O:Ch" ) ) != -1 ) {
        switch ( opt ) {
            case 'p':
                perlfile = optarg;
                break;
            case 't':
                testfile = optarg;
                break;
            case 'c':
                compile_file = optarg;
                break;
            case 'e':
                eval = optarg;
                break;
            case 'C':
                do_cover = 1;
                break;
            case 'M':
                module = optarg;
                break;
            case 'O':
                cover_args = optarg;
                break;
            case 'h':
            default:
                usage();
        }
    }

    if ( ( compile_file ) && ( ( perlfile != NULL ) || ( testfile != NULL ) ) ) {
        usage();
    }

    if ( compile_file ) {
        /* check_script will exit for us. */
        check_script( compile_file );
        /* NOT REACHED */
        return 0;
    }

    if ( ( perlfile == NULL ) || ( testfile == NULL ) ) {
        usage();
    }

    if ( exec_self_for_check( testfile ) ) {
        errx( 1, "Test script failed to compile, aborting." );
    }

    return init_perl_test_framework( perlfile, testfile, module, eval, env, do_cover, cover_args );
}

