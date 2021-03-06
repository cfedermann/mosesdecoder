#!/usr/bin/perl -w

# $Id$
use strict;
use Getopt::Long "GetOptions";

binmode(STDIN, ":utf8");
binmode(STDOUT, ":utf8");

# apply switches
my ($DIR,$CORPUS,$SCRIPTS_ROOT_DIR,$CONFIG,$HELP,$ERROR);
my $LM = "SRILM"; # SRILM is default.
my $BUILD_LM = "build-lm.sh";
my $NGRAM_COUNT = "ngram-count";
my $TRAIN_SCRIPT = "train-factored-phrase-model.perl";
my $MAX_LEN = 1;
my $FIRST_STEP = 1;
my $LAST_STEP = 11;
$ERROR = "training Aborted."
    unless &GetOptions('first-step=i' => \$FIRST_STEP,
                       'last-step=i' => \$LAST_STEP,
                       'corpus=s' => \$CORPUS,
                       'config=s' => \$CONFIG,
                       'dir=s' => \$DIR,
                       'ngram-count=s' => \$NGRAM_COUNT,
                       'build-lm=s' => \$BUILD_LM,
                       'lm=s' => \$LM,
                       'train-script=s' => \$TRAIN_SCRIPT,
                       'scripts-root-dir=s' => \$SCRIPTS_ROOT_DIR,
                       'max-len=i' => \$MAX_LEN,
                       'help' => \$HELP);

# check and set default to unset parameters
$ERROR = "please specify working dir --dir" unless defined($DIR) || defined($HELP);
$ERROR = "please specify --corpus" if !defined($CORPUS) && !defined($HELP) 
                                  && $FIRST_STEP <= 2 && $LAST_STEP >= 1;

if ($HELP || $ERROR) {
    if ($ERROR) {
        print STDERR "ERROR: " . $ERROR . "\n";
    }
    print STDERR "Usage: $0 --dir /output/recaser --corpus /Cased/corpus/files [options ...]";

    print STDERR "\n\nOptions:
  == MANDATORY ==
  --dir=dir                 ... outputted recaser directory.
  --corpus=file             ... inputted cased corpus.

  == OPTIONAL ==
  = Recaser Training configuration =
  --train-script=file       ... path to the train script (default: train-factored-phrase-model.perl in \$PATH).
  --config=config           ... training script configuration.
  --scripts-root-dir=dir    ... scripts directory.
  --max-len=int             ... max phrase length (default: 1).

  = Language Model Training configuration =
  --lm=[IRSTLM,SRILM]       ... language model (default: SRILM).
  --build-lm=file           ... path to build-lm.sh if not in \$PATH (used only with --lm=IRSTLM).
  --ngram-count=file        ... path to ngram-count.sh if not in \$PATH (used only with --lm=SRILM).

  = Steps this script will perform =
  (1) Truecasing (disabled);
  (2) Language Model Training;
  (3) Data Preparation
  (4-10) Recaser Model Training; 
  (11) Cleanup.
  --first-step=[1-11]       ... step where script starts (default: 1).
  --last-step=[1-11]        ... step where script ends (default: 11).

  --help                    ... this usage output.\n";
  if ($ERROR) {
    exit(1);
  }
  else {
    exit(0);
  }
}

# main loop
`mkdir -p $DIR`;
&truecase()           if 0 && $FIRST_STEP == 1;
&train_lm()           if $FIRST_STEP <= 2;
&prepare_data()       if $FIRST_STEP <= 3 && $LAST_STEP >= 3;
&train_recase_model() if $FIRST_STEP <= 10 && $LAST_STEP >= 3;
&cleanup()            if $LAST_STEP == 11;

### subs ###

sub truecase {
    # to do
}

sub train_lm {
    print STDERR "(2) Train language model on cased data @ ".`date`;
    my $cmd = "";
    if (uc $LM eq "IRSTLM") {
        $cmd = "$BUILD_LM -t /tmp -i $CORPUS -n 3 -o $DIR/cased.irstlm.gz";
    }
    else {
        $LM = "SRILM";
        $cmd = "$NGRAM_COUNT -text $CORPUS -lm $DIR/cased.srilm.gz -interpolate -kndiscount";
    }
    print STDERR "** Using $LM **" . "\n";
    print STDERR $cmd."\n";
    system($cmd) == 0 || die("Language model training failed with error " . ($? >> 8) . "\n");
}

sub prepare_data {
    print STDERR "\n(3) Preparing data for training recasing model @ ".`date`;
    open(CORPUS,$CORPUS);
    binmode(CORPUS, ":utf8");
    open(CASED,">$DIR/aligned.cased");
    binmode(CASED, ":utf8");
    print "$DIR/aligned.lowercased\n";
    open(LOWERCASED,">$DIR/aligned.lowercased");
    binmode(LOWERCASED, ":utf8");
    open(ALIGNMENT,">$DIR/aligned.a");
    while(<CORPUS>) {
	next if length($_)>2000;
	s/\x{0}//g;
	s/\|//g;
	s/ +/ /g;
	s/^ //;
	s/ [\r\n]*$/\n/;
	next if /^$/;
	print CASED $_;
	print LOWERCASED lc($_);
	my $i=0;
	foreach (split) {
	    print ALIGNMENT "$i-$i ";
	    $i++;
	}
	print ALIGNMENT "\n";
    }
    close(CORPUS);
    close(CASED);
    close(LOWERCASED);
    close(ALIGNMENT);
}

sub train_recase_model {
    my $first = $FIRST_STEP;
    $first = 4 if $first < 4;
    print STDERR "\n(4) Training recasing model @ ".`date`;
    my $cmd = "$TRAIN_SCRIPT --root-dir $DIR --model-dir $DIR --first-step $first --alignment a --corpus $DIR/aligned --f lowercased --e cased --max-phrase-length $MAX_LEN";
    if (uc $LM eq "IRSTLM") {
        $cmd .= " --lm 0:3:$DIR/cased.irstlm.gz:1";
    }
    else {
        $cmd .= " --lm 0:3:$DIR/cased.srilm.gz:0";
    }
    $cmd .= " -config $CONFIG" if $CONFIG;
    print STDERR $cmd."\n";
    system($cmd) == 0 || die("Recaser model training failed with error " . ($? >> 8) . "\n");
}

sub cleanup {
    print STDERR "\n(11) Cleaning up @ ".`date`;
    `rm -f $DIR/extract*`;
    my $clean_1 = $?;
    `rm -f $DIR/aligned*`;
    my $clean_2 = $?;
    `rm -f $DIR/lex*`;
    my $clean_3 = $?;
    if ($clean_1 + $clean_2 + $clean_3 != 0) {
        print STDERR "Training successful but some files could not be cleaned.\n";
    }
}
