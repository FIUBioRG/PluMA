use File::Basename;
use Data::Dumper;

BEGIN{
	my $script_directory = &File::Basename::dirname( $0 );
	unshift @INC, $script_directory;
}

package vPhyloMM_GUI;

use strict;
use warnings;
use Tkx;
use File::Basename;
use vPhyloMM qw(vPhyloMM->setup_output vPhyloMM->setup_input vPhyloMM->setup_variable);
use feature 'say';

sub new{
	
	my $package = shift;
	my %parameters = @_;
	my $self = {};
	
	bless $self, $package;
	
	vPhyloMM->setup_input( \my $log, $parameters{Log}, \*STDERR ) and do{
		print STDERR "Error: vPhyloMM_GUI could not set up log output\n";
		return -1;
	};
	
	vPhyloMM->setup_variable( \my $verbose, $parameters{Verbose}, 0 ) and do{
		print $log "Error: vPhyloMM_GUI could not set up variable 'verbose'\n";
		undef $log;
		return -1;
	};
	
	$self->{Verbose} = $verbose;
	$self->{Log} = $log;
	print $log "vPhyloMM_GUI is instantiating...\n" if $verbose > 1;
	
	vPhyloMM->setup_variable( \my $variables_file_input, $parameters{Variables_File_Input} ) and do{
		print $log "Error: vPhyloMM_GUI did not receive variables file name\n" if $verbose;
		undef $log;
		return -1;
	};
	$self->{Variables_File_Input} = $variables_file_input;
	
	$self->{Variables_File_Input} = File::Spec->rel2abs( $self->{Variables_File_Input} ) if $self->{Variables_File_Input};
	
	say $log "using '$variables_file_input'" if $verbose;
	
	print $log "vPhyloMM_GUI is instantiated\n" if $verbose > 1;
	
	undef $log;
	
	return $self;
	
}

sub run{
	
	my $self = shift;
	my %parameters = @_;
	
	vPhyloMM->setup_log( \my $log, $parameters{Log}, \*STDERR ) and do{
		say STDERR "Error: vPhyloMM_GUI could not set up log output"; 
		return -1;
	};
	vPhyloMM->setup_variable( \my $verbose, $parameters{Verbose}, $self->{Verbose}, 0 ) and do{
		say $log "Error: vPhyloMM_GUI could not set up variable";
		undef $log;
		return -1;
	};
	vPhyloMM->setup_variable( \my $variables_file, $parameters{Variables_File_Input}, $self->{Variables_File_Input} ) and do{
		print $log "Error: vPhyloMM_GUI did not receive variables file name\n" if $verbose;
		undef $log;
		return -1;
	};
	
	say $log "vPhyloMM_GUI is running..." if $verbose;
	Tkx::package( "require", "treectrl" );
	Tkx::option_add( "*tearOff", 0 );
	$self->{Main_Window} = Tkx::widget->new( '.' );
	$self->{Main_Window}->g_wm_protocol( 'WM_DELETE_WINDOW', [ sub{ return $self->exit_program(); } ] );
	Tkx::bind( $self->{Main_Window}, "<Destroy>", [ sub{ if( $self->{Main_Window} eq shift ){ say $log "the GUI is closing..."; return 0; } }, Tkx::Ev("%W") ] );
	Tkx::wm_title( $self->{Main_Window}, "vPhyloMM" );
	$self->{Main_Window}->g_wm_geometry("500x600");
	my $windowingsystem = Tkx::tk('windowingsystem');
	$self->{Icon_Data} = qq(R0lGODlhEAAQAPcAAAAAAAEBAQICAgMDAwQEBAUFBQYGBgcHBwgICAkJCQoKCgsLCwwMDA0NDQ4ODg8PDxAQEBERERISEhMTExQUFBUVFRYWFhcXFxgYGBkZGRoaGhsbGxwcHB0dHR4eHh8fHyAgICEhISIiIiMjIyQkJCUlJSYmJicnJygoKCkpKSoqKisrKywsLC0tLS4uLi8vLzAwMDExMTIyMjMzMzQ0NDU1NTY2Njc3Nzg4ODk5OTo6Ojs7Ozw8PD09PT4+Pj8/P0BAQEFBQUJCQkNDQ0REREVFRUZGRkdHR0hISElJSUpKSktLS0xMTE1NTU5OTk9PT1BQUFFRUVJSUlNTU1RUVFVVVVZWVldXV1hYWFlZWVpaWltbW1xcXF1dXV5eXl9fX2BgYGFhYWJiYmNjY2RkZGVlZWZmZmdnZ2hoaGlpaWpqamtra2xsbG1tbW5ubm9vb3BwcHFxcXJycnNzc3R0dHV1dXZ2dnd3d3h4eHl5eXp6ent7e3x8fH19fX5+fn9/f4CAgIGBgYKCgoODg4SEhIWFhYaGhoeHh4iIiImJiYqKiouLi4yMjI2NjY6Ojo+Pj5CQkJGRkZKSkpOTk5SUlJWVlZaWlpeXl5iYmJmZmZqampubm5ycnJ2dnZ6enp+fn6CgoKGhoaKioqOjo6SkpKWlpaampqenp6ioqKmpqaqqqqurq6ysrK2tra6urq+vr7CwsLGxsbKysrOzs7S0tLW1tba2tre3t7i4uLm5ubq6uru7u7y8vL29vb6+vr+/v8DAwMHBwcLCwsPDw8TExMXFxcbGxsfHx8jIyMnJycrKysvLy8zMzM3Nzc7Ozs/Pz9DQ0NHR0dLS0tPT09TU1NXV1dbW1tfX19jY2NnZ2dra2tvb29zc3N3d3d7e3t/f3+Dg4OHh4eLi4uPj4+Tk5OXl5ebm5ufn5+jo6Onp6erq6uvr6+zs7O3t7e7u7u/v7/Dw8PHx8fLy8vPz8/T09PX19fb29vf39/j4+Pn5+fr6+vv7+/z8/P39/f7+/v///ywAAAAAEAAQAAAIhwABWPpHsCA4AKQKErxjQyFBUgDUOVQGwJdDKIccEoRyRyFFZRr/xQKwruAhKCH/2bMhi2A9lin/IUL5T5YNOQBy5rxTj2A1ANX+lUF0pyPBdU+MCrV00FpRhbIAFIwFheq/p1OlFrRRoyXWf+GgXFJIyoa9qzpzXuoZ8mvMhUrfXo371i3BgAA7);
	Tkx::wm_iconphoto( $self->{Main_Window}, Tkx::image_create_photo( -data => $self->{Icon_Data}, -format => 'GIF' ) );
	$self->create_menu( Variables_File_Input => $variables_file, Log => $log ) and do{
		say $log "Error: Couldn't create the file menu\n" if $verbose;
		return -1;
	};
	$self->{Main_Window}->g_grid_columnconfigure( 0, -weight => 1 );
	$self->{Main_Window}->g_grid_rowconfigure( 0, -weight => 1 );
	$self->{Main_Window}->g_grid_columnconfigure( 1, -weight => 0 );
	$self->{Main_Window}->g_grid_rowconfigure( 1, -weight => 0 );
	($self->{Main_Window}->new_ttk__sizegrip)->g_grid( -column => 1, -row => 1, -sticky => "se" );
	my $paned_window = $self->{Main_Window}->new_ttk__panedwindow( -orient => 'vertical', );
	my $edit_variables_frame = $paned_window->new_ttk__labelframe(
		-text => "edit variables",
	);
	$edit_variables_frame->g_grid_columnconfigure( 0, -weight => 1 );
	$edit_variables_frame->g_grid_rowconfigure( 0, -weight => 1 );
	my $generate_reports_label_frame = $paned_window->new_ttk__labelframe(
		-text => "generate reports",
	);
	$paned_window->add( $edit_variables_frame, -weight => "1",);
	$paned_window->add( $generate_reports_label_frame, -weight => "0", );
	$paned_window->g_grid(
		-row => "0",
		-column => "0",
		-sticky => "nsew",
	);
	$self->{Progress} = 0;
	my $progress_bar = $generate_reports_label_frame->new_ttk__progressbar(-orient => 'horizontal',-mode => 'determinate', );
	$self->{Edit_Variables_Frame} = $edit_variables_frame;
	$self->{Generate_Reports_Frame} = $generate_reports_label_frame;
	my $button = $generate_reports_label_frame->new_ttk__button(
		-text => "go",
		-width => 20,
		-command => sub {$self->generate_reports();$self->{Progress_Bar}->configure(-value => 0);$self->{Report_Status}='';$self->{Cancel_Button_Window}->g_destroy();});
	$self->{Report_Status} = "";
	my $status = $generate_reports_label_frame->new_ttk__label(-textvariable => \$self->{Report_Status}, -width => 7, );
	$self->{Status} = $status;
	$status->g_grid(
		-column => 2,
		-row => 1,
	);
	$button->g_grid(
		-column => 0,
		-row => 0,
		-columnspan => 2,
	);
	$generate_reports_label_frame->g_grid_columnconfigure(0, -weight => 0);
	$generate_reports_label_frame->g_grid_columnconfigure(1, -weight => 1);
	$generate_reports_label_frame->g_grid_columnconfigure(1, -weight => 1);
	$generate_reports_label_frame->g_grid_columnconfigure(1, -weight => 1);
	$progress_bar->g_grid(
		-column => 1,
		-row => 1,
		-sticky => "we",
	);
	$self->{Progress_Bar} = $progress_bar;
	$self->new_file(
		Variables_File_Input => $variables_file,
	);
	Tkx::MainLoop();
	say $log "done running GUI" if $verbose;
	undef $log;
	return 0;	
}




sub create_menu{
	
	my $self = shift;
	my %parameters = @_;
	
	vPhyloMM->setup_log( \my $log, $parameters{Log}, \*STDERR ) and do{
		say STDERR "Error: Could not set up log output";
		return -1;
	};
	vPhyloMM->setup_variable( \my $verbose, $parameters{Verbose}, $self->{Verbose}, 0 ) and do{
		say $log "Error: Could not set variable 'verbose'";
		undef $log;
		return -1;
	};
	vPhyloMM->setup_variable( \my $variables_file, $parameters{Variables_File_Input}, $self->{Variables_File_Input} ) and do{
		say $log "Error: did not receive variables input file" if $verbose;
		undef $log;
		return -1;
	};
	
	say $log "Creating file menu..." if $verbose > 1;
	
	my $m = $self->{Main_Window}->new_menu;
	my $file_menu = $m->new_menu;
	
	$file_menu->add_command(
		-label => "New",
		-command => sub{ $self->new_file( Variables_File_Input => $variables_file ); },
	);
	
	$file_menu->add_command(
		-label => "Open...",
		-command => sub { $self->open_file(); },
	);
	
	$file_menu->add_command(
		-label => "Save...",
		-command => sub { $self->save(); },
	);
	
	$file_menu->add_command(
		-label => "Save as...",
		-command => sub { $self->save_as(); }
	);
		
	$file_menu->add_separator;
	
	$file_menu->add_command(
		-label => "Exit",
		-command => sub { $self->exit_program(); }
	);
	
	$m->add_cascade(
		-menu => $file_menu,
		-label => "File"
	);
	
	$self->{Main_Window}->configure(
		-menu => $m,
	);
	
	print $log "done creating file menu\n" if $verbose > 1;
	undef $log;
	return 0;
}


sub save{
	my $self = shift;
	my %parameters = @_;
	my $file_name = $self->{Save_File_Name};
	if ( !$file_name ){
		$file_name = Tkx::tk___getSaveFile( -title => "Save...") or return -1;
	}
	open( FILE, ">".$file_name ) or do{
		Tkx::tk___messageBox(
		    -message => "Couldn't save to \"$file_name\" $!",
		    -icon => "error",
		    -title => "Error",
		);
		return -1;
	};
	$self->{Save_File_Name} = $file_name;
	$self->recurse_save(
		Hash => $self->{Object}->{Variables_Tree},
		Output => \*FILE,
	);

	close FILE;
	
	$self->{Saved} = 1;
	Tkx::wm_title( $self->{Main_Window}, "vPhyloMM - " . basename($file_name) );
	return 0;
	
}

sub save_as{
	my $self = shift;
	my %parameters = @_;
	my $file_name = Tkx::tk___getSaveFile( -title => "Save as...") or return -1;
	open( FILE, ">".$file_name ) or do{
		Tkx::tk___messageBox(
		    -message => "Couldn't save to \"$file_name\" $!",
		    -icon => "error",
		    -title => "Error",
		);
		return -1;
	};
	$self->{Save_File_Name} = $file_name;
	$self->recurse_save(
		Hash => $self->{Object}->{Variables_Tree},
		Output => \*FILE,
	);

	close FILE;
	
	$self->{Saved} = 1;
	Tkx::wm_title( $self->{Main_Window}, "vPhyloMM - " . basename($file_name) );
	return 0;
	
}

sub new_file{
	my $self = shift;
	my %parameters = @_;
	vPhyloMM->setup_variable( \my $variables_file, $parameters{Variables_File_Input}, $self->{Variables_File_Input} ) and return -1;
	return -1 if vPhyloMM->setup_log( \my $log, $parameters{Log}, \*STDERR );
	return -1 if vPhyloMM->setup_variable( \my $title, $parameters{Title}, $self->{Title}, '' );
	return -1 if vPhyloMM->setup_variable( \my $verbose, $parameters{Verbose}, $self->{Verbose}, 0 );
	if( exists( $self->{Saved} ) && !$self->{Saved} ){
		my $response = Tkx::tk___messageBox(
			-type => "yesnocancel",
			-message => "There may be unsaved changed. Are you sure you'd like to open a new file?",
			-icon => "question",
			-title => "discard changes?"
		);
		if( $response ne 'yes'){
			return 0;				
		}
	}
	my $vPhyloMM = vPhyloMM->new(
		Log => $log,
		Variables_File_Input => $variables_file,
		Verbose => $verbose,
	);
	my $ref = ref $vPhyloMM;
	if( $ref ne "vPhyloMM" ){
		print $log "Error: could not create a vPhyloMM object ($vPhyloMM)\n" if $verbose;
		undef $log;
		return -1;
	}
	$self->{Object} = $vPhyloMM;
	my $variables_hash = $vPhyloMM->{Variables_Tree};
	my $edit_variables_frame = $self->{Edit_Variables_Frame};
	my $generate_reports_label_frame = $self->{Generate_Reports_Frame};
	if( $self->{Tree} ){
		$self->{Tree}->g_grid_remove( );
	}
	my $tree = $edit_variables_frame->new_treectrl(
		-doublebuffer => "window",
	);
	$tree->column_create(
		-text => "variable",
		-expand => "no",
		-tags => "C0",
	);
	$tree->column_create(
		-text => "value",
		-tags => "C1",
		-expand => "yes",
		-squeez => "yes",
		-itemjustify => "left",
	);
	$tree->configure(
		-treecolumn => "C0",
	);
	my $elemLabel = $tree->element_create(
		"elemLabel",
		"text",
	);
	
	my $elemWindow = $tree->element_create(
		"elemWindow",
		"window",
	);
	
	my $elemWindow1 = $tree->element_create(
		"elemWindow1",
		"window",
	);
	
	my $styleValue = $tree->style_create(
		"styleValue",
	);
	
	my $styleLabel = $tree->style_create(
		"styleLabel",
	);
	
	$tree->style_elements(
		"styleValue",
		"elemWindow",
	);
	
	$tree->style_elements(
		"styleValue",
		"elemWindow elemWindow1",
	);
	
	$tree->style_elements(
		"styleLabel",
		"elemLabel",
	);
	
	$tree->style_layout(
		"styleValue",
		"elemWindow",
		-iexpand => "x",
		-squeeze => "x",
		-sticky => "nwes",
	);
	
	my $scroll_bar = $edit_variables_frame->new_ttk__scrollbar(
		-command => [$tree, "yview"],
		-orient => "vertical",
	);
	$tree->g_grid(
		-column => "0",
		-row => "0",
		-sticky => "nsew",
	);
	$tree->configure(
		-yscrollcommand => [$scroll_bar, "set"]
	);
	$scroll_bar->g_grid(
		-column => 1,
		-row => 0,
		-sticky => "ns",
	);
	
	$self->recurse_hash(
		"Variables_Hash" => $variables_hash,
		"Tree" => $tree,
	);
	
	
	$self->{Tree} = $tree;
	$self->{Save_File_Name} = '';
	$self->{Save_File_Name} = $variables_file if $title;
	$title = 'untitled' unless $title;
	Tkx::wm_title( $self->{Main_Window}, "vPhyloMM - $title" );
	$self->{Saved} = 1;
	return 0;
}

sub validate{
	my $self = shift;
	my( $name, $after, $before ) = @_;
	my $vPhyloMM = $self->{Object};
	$vPhyloMM->set( $name, $after );
	return 1; 
}

sub open_file{
	my $self = shift;
	my %parameters = @_;
	my $file = Tkx::tk___getOpenFile();
	return 0 if !$file;
	return 0 unless -e $file;
	my $title = basename $file;
	return $self->new_file( Variables_File_Input => $file, Title => $title );
}

sub recurse_hash{
	
	my $self = shift;
	my %parameters = @_;
	
	vPhyloMM->setup_log( \my $log, $parameters{Log}, \*STDERR ) and do{
		print STDERR "Error: Could not set up log output\n";
		return -1;
	};
	vPhyloMM->setup_variable( \my $verbose, $parameters{Verbose}, $self->{Verbose}, 0 ) and do{
		print $log "Error: Could not set variable\n";
		undef $log;
		return -1;
	};
	vPhyloMM->setup_variable( \my $level, $parameters{Level}, 0 ) and do{
		print $log "Error: Could not set variable\n";
		undef $log;
		return -1;
	};
	
	my $hash = $parameters{"Variables_Hash"};
	my $tree = $parameters{"Tree"};
	my $root_name;
	my $name = $hash->{"Name"};
	my $path;
	my $current_parent;
	my $tabs = "\t" x ( $level );
	
	if( !exists $parameters{"Root_Name"} ){
		print $log "recursing hash...\n" if $verbose > 1;
		$path = $name;
		$current_parent = $tree->item_id( "root" );
	}
	
	else{
		$root_name = $parameters{"Root_Name"};
		$path = "$root_name.$name";
		$current_parent = $tree->item_id( "tag $path" );
	}
	
	for my $group ( @{$hash->{"Groups"}} ){
		my $name = $group->{"Name"};
		my $item = $tree->item_create( -tag => "$path.$name" );
		$tree->item_configure( $item, -button => "auto" );
		$tree->item_style_set( $item, "C0", "styleLabel" );
		$tree->item_element_configure( $item, "C0", "elemLabel", -text => "$name" );
		$tree->item_collapse( $item ) unless $name eq 'program_options';
		$tree->item_lastchild( $current_parent, $item );
		$self->recurse_hash(
			"Variables_Hash" => $group,
			"Root_Name" => $path,
			"Tree" => $tree,
			"Level" => $level + 1,
		);
	}
	
	for my $variable ( @{$hash->{"Variables"}} ){
		my $variable_name = $variable->{"Label"} ? $variable->{"Label"} : $variable->{"Name"};
		my $name = $variable->{Name};
		my $frame = $tree->new_ttk__frame;
		say $log "$tabs$path.$variable_name" if $verbose > 1;
		my $item = $tree->item_create( -tag => "$path.$variable_name" );
		$tree->item_style_set( $item, "C0", "styleLabel" );
		$tree->item_style_set( $item, "C1", "styleValue" );
		my $e = $frame->new_ttk__entry( -textvariable => \$variable->{"Value"}, -validate => 'key', -validatecommand => [sub{ $self->validate( $variable->{"Name"}, @_ ); 1;}, Tkx::Ev( "%P", "%s" )] );
		my $button = "";
		my $type = $variable->{Type} ? $variable->{Type} : "";
		my $combo_box = '';
		my $check_box = '';
		
		if( $name =~ /Directory$/ ){
			$button = $frame->new_ttk__button(
				-text => "...",
				-command => sub{
					my $file = Tkx::tk___chooseDirectory( -title => $variable->{Label} ? $variable->{Label} : $variable->{Name}, -mustexist => '0' );
					return 0 if !$file;
					if( !-d $file ){
						my $response = Tkx::tk___messageBox(
							-type => "yesnocancel",
							-message => "The directory you selected doesn't exist; would you like to create \"$file\"?",
							-icon => "question",
							-title => "Create directory..."
						);
						return 0 if $response eq 'cancel' or $response eq 'no';
						if( !mkdir $file ){
							Tkx::tk___messageBox(
								-message => "The following directory could not be created: \"$file\": $!",
								-icon => "error",
								-title => "Create directory..."
							);
							return 0;
						}
					}
					return if !$file;
					$variable->{Value} = $file;
					$self->{Object}{$name} = $file;
					$self->{Saved} = 0;
				},
			);
		}
		elsif( $name =~ /File_Output$/ ){
			$button = $frame->new_ttk__button(
				-text => "...",
				-command => sub{
					my $file = Tkx::tk___getSaveFile( -title => $variable->{Label} ? $variable->{Label} : $variable->{Name}, );
					return if !$file;
					$variable->{Value} = $file;
					$self->{Object}->{$name} = $file;
					$self->{Saved} = 0;
				},
			);
		}
		elsif( $name =~ /File_Input$/ ){
			$button = $frame->new_ttk__button(
				-text => "...",
				-command => sub{
					my $file = Tkx::tk___getOpenFile( -title => $variable->{Label} ? $variable->{Label} : $variable->{Name}, );
					return if !$file;
					$variable->{Value} = $file;
					$self->{Object}->{$name} = $file;
					$self->{Saved} = 0;
				},
			);
		}
		elsif( exists $variable->{Options} ){
			$combo_box  = $frame->new_ttk__combobox( -textvariable => \$variable->{Key} );
			$combo_box->configure( -values => [keys %{$variable->{Options}}] );
			$combo_box->configure( -state => 'readonly' );
			my $default_value = [keys %{$variable->{Options}}]->[0];
			if( exists $variable->{Key} ){
				$default_value = $variable->{Key};
			}
			$variable->{Value} = $variable->{Options}->{$default_value};
			$variable->{Key} = $default_value;
			$combo_box->set($default_value);
			$combo_box->g_bind("<<ComboboxSelected>>", sub { $self->{Saved} = 0; $variable->{Value} = $variable->{Options}{$variable->{Key}}; $self->{Object}->{$name} = $variable->{Options}{$variable->{Key}}; });
		}
		elsif( exists $variable->{Type} && $variable->{Type} eq "check_box" ){
			$check_box = $frame->new_ttk__checkbutton(-text => "", -command => sub{$self->{Saved} = 0; $self->{Object}{$variable->{Name}} = $variable->{Value}; }, -variable => \$variable->{Value}, -onvalue => "1", -offvalue => "0");
		}
		
		if( $button && $e ){
			$e->g_grid(
				-column => 0,
				-row => 0,
				-sticky => "ew",
			);
			$button->g_grid(
				-column => 1,
				-row => 0,
				-sticky => "e",
			);
		}
		elsif( $combo_box ){
			$combo_box->g_grid(
				-column => 0,
				-row => 0,
				-sticky => "ew",
			);
		}
		elsif( $check_box ){
			$check_box->g_grid(
				-column => 0,
				-row => 0,
			);
		}
		elsif( $e ){
			$e->g_grid(
				-column => 0,
				-row => 0,
				-sticky => "ew",
			);
		}
		$button->configure( -width => 3 ) if $button;
		$frame->g_grid_columnconfigure( 0, -weight => 1 );
		$frame->g_grid_rowconfigure( 0, -weight => 1 );
		$frame->g_grid_columnconfigure( 1, -weight => 0 );
		$frame->g_grid_rowconfigure( 0, -weight => 0 );
		$tree->item_element_configure( $item, "C0", "elemLabel", -text => "$variable_name" );
		$tree->item_element_configure( $item, "C1", "elemWindow", -window => $frame,  );# unless $combo_box || $check_box;
		#$tree->item_element_configure( $item, "C1", "elemWindow1", -window => $button,  ) if $button;
		#$tree->item_element_configure( $item, "C1", "elemWindow1", -window => $combo_box,  ) if $combo_box;
		#$tree->item_element_configure( $item, "C1", "elemWindow1", -window => $check_box,  ) if $check_box;
		$tree->item_lastchild( $current_parent, $item );
	}
	
	if( !exists $parameters{"Root_Name"} ){
		print $log "done recursing hash...\n" if $verbose > 1;
	}
	
	undef $log;
	return 0;
	
}

sub recurse_save{
	
	my $self = shift;
	my %parameters = @_;
	my $OUTPUT = $parameters{"Output"};
	my $level = $parameters{"Level"} ? $parameters{"Level"} : 0;
	my $hash = $parameters{"Hash"};
	
	for my $group ( @{$hash->{"Groups"}} ){
		my $name = $group->{"Name"};
		print $OUTPUT "\t" x $level . "<$name>\n";
		$self->recurse_save(
			"Hash" => $group,
			"Output" => $OUTPUT,
			"Level" => $level + 1,
		);
		print $OUTPUT "\t" x $level . "</$name>\n";
	}
	
	for my $variable ( @{$hash->{"Variables"}} ){
		my @values;
		my $tabs = "\t" x ( $level + 1 );
		for my $key ( keys %$variable ){
			if( ref( $variable->{$key} ) eq 'HASH' ){
				my $hash_string = Data::Dumper->Dumper($variable->{$key});
				$hash_string =~ s/^[^{]*//;
				$hash_string =~ s/[^}]*$//;
				$hash_string =~ s/\s//g;
			}
		}
		for my $key ( keys %$variable ){
			if( ref( $variable->{$key} ) eq 'HASH' ){
				my $hash_string = Data::Dumper->Dumper($variable->{$key});
				$hash_string =~ s/^[^{]*//;
				$hash_string =~ s/[^}]*$//;
				$hash_string =~ s/^\s+//;
				$hash_string =~ s/\s+$//;
				$hash_string =~ s/\n//g;
				push @values, qq( $key  => $hash_string, );
				next;
			}
			push @values, qq( $key => "$variable->{$key}", );
		}
		print $OUTPUT "$tabs\{" . join ( "\n$tabs", @values ) . "}\n\n";
	}
	
	return 0;
	
}

sub generate_reports{
	my $self = shift;
	my %parameters = @_;
	vPhyloMM->setup_variable( \my $verbose, $parameters{Verbose}, $self->{Verbose}, 0 );
	my $vPhyloMM = $self->{Object};
	my $log;
	my $count = 0;
	my $transitions = 1;
	my $cancel_window = $self->{Main_Window}->new_toplevel;
	$self->{Cancel_Button_Window} = $cancel_window;
	$self->{Cancel} = 0;
	my $cancel_button = $cancel_window->new_ttk__button(-text => "cancel", -width=>15, -command => sub {$self->{Cancel} = 1;}, );
	$cancel_button->g_grid();
	Tkx::update();
	$vPhyloMM->{Verbose} = $self->{Verbose};
	$vPhyloMM->{Output_Directory} = File::Spec->rel2abs( $vPhyloMM->{Output_Directory} );
	if( ! -e $vPhyloMM->{Output_Directory} ){
		if( !mkdir $vPhyloMM->{Output_Directory} ){
			Tkx::tk___messageBox(
				-message => "The directory '$vPhyloMM->{Output_Directory}' does not exists and couldn't be created. Check permissions.",
				-icon => "error",
				-title => "Error...",
			);
			return -1;
		}
	}
	$vPhyloMM->{Log} = File::Spec->catfile( File::Spec->rel2abs( $vPhyloMM->{Output_Directory} ), "log.txt" );
	open( $log, ">" , $vPhyloMM->{Log} ) or do{
		Tkx::tk___messageBox(
			-message => "The file '$vPhyloMM->{Log}' couldn't be opened for writing. Check permissions.",
			-icon => "error",
			-title => "Error...",
		);
		return -1;
	};
	close $log;
	my @reports = qw(
		Report_SlidingMinPD
		Report_Transitions
		Report_Aggregates
		Report_Summary
		Report_R
		Report_Graphs
		Report_Shortest_Paths
		Report_Pruned_R
		Report_Pruned_Shortest_Paths
		Report_Pruned_Graphs
	);
	
	my $dna_count = 0;
	map {$count++ if $vPhyloMM->{$_} } @reports;
	$vPhyloMM->{SlidingMinPD_Directory} = File::Spec->catfile( $vPhyloMM->{Output_Directory}, "Sliding MinPD" );
	$vPhyloMM->{Pruned_Directory} =File::Spec->catfile( $vPhyloMM->{Output_Directory}, "Pruned" );
	if( ! -e $vPhyloMM->{Pruned_Directory} && ( $vPhyloMM->{Report_Pruned_R} || $vPhyloMM->{Report_Pruned_Shortest_Paths} || $vPhyloMM->{Report_Pruned_Graphs} ) ){
		if( !mkdir $vPhyloMM->{Pruned_Directory} ){
			Tkx::tk___messageBox(
				-message => "The directory '$vPhyloMM->{Pruned_Directory}' does not exists and couldn't be created. Check permissions.",
				-icon => "error",
				-title => "Error...",
			);
			return -1;
		}
	}
	$vPhyloMM->{Shortest_Paths_Directory} =File::Spec->catfile( $vPhyloMM->{Output_Directory}, "Shortest Paths" );
	if( ! -e $vPhyloMM->{Shortest_Paths_Directory}  && ( $vPhyloMM->{Report_Shortest_Paths} ) ){
		if( !mkdir $vPhyloMM->{Shortest_Paths_Directory} ){
			Tkx::tk___messageBox(
				-message => "The directory '$vPhyloMM->{Shortest_Paths_Directory}' does not exists and couldn't be created. Check permissions.",
				-icon => "error",
				-title => "Error...",
			);
			return -1;
		}
	}
	$vPhyloMM->{Graph_Directory} =File::Spec->catfile( $vPhyloMM->{Output_Directory}, "Pathway Graphs" );
	if( ! -e $vPhyloMM->{Graph_Directory} && $vPhyloMM->{Report_Graphs} ){
		if( !mkdir $vPhyloMM->{Graph_Directory} ){
			Tkx::tk___messageBox(
				-message => "The directory '$vPhyloMM->{Graph_Directory}' does not exists and couldn't be created. Check permissions.",
				-icon => "error",
				-title => "Error...",
			);
			return -1;
		}
	}
	$vPhyloMM->{Pruned_Graphs_Directory} = File::Spec->catfile( $vPhyloMM->{Pruned_Directory}, "Pathway Graphs" );
	if( ! -e $vPhyloMM->{Pruned_Graphs_Directory} && $vPhyloMM->{Report_Pruned_Graphs} ){
		if( !mkdir $vPhyloMM->{Pruned_Graphs_Directory} ){
			Tkx::tk___messageBox(
				-message => "The directory '$vPhyloMM->{Pruned_Graphs_Directory}' does not exists and couldn't be created. Check permissions.",
				-icon => "error",
				-title => "Error...",
			);
			return -1;
		}
	}
	$vPhyloMM->{Pruned_Shortest_Path_Directory} =File::Spec->catfile( $vPhyloMM->{Pruned_Directory}, "Shortest Paths" );
	if( ! -e $vPhyloMM->{Pruned_Shortest_Path_Directory} && $vPhyloMM->{Report_Pruned_Shortest_Paths} ){
		if( !mkdir $vPhyloMM->{Pruned_Shortest_Path_Directory} ){
			Tkx::tk___messageBox(
				-message => "The directory '$vPhyloMM->{Pruned_Shortest_Path_Directory}' does not exists and couldn't be created. Check permissions.",
				-icon => "error",
				-title => "Error...",
			);
			return -1;
		}
	}
	if( $vPhyloMM->{Report_SlidingMinPD} ){
		opendir( DNA_DIR, $vPhyloMM->{DNA_Directory} ) or do{
			Tkx::tk___messageBox(
			    -message => "Sliding MinPD did not run successfully. Check the log file.",
			    -icon => "error",
			    -title => "Error...",
			);
			return -1;	
		};
		say $vPhyloMM->{DNA_Directory};
		for my $base ( readdir DNA_DIR ){
			my $file = File::Spec->catfile( $vPhyloMM->{DNA_Directory}, $base );
			next if -d $file;
			$dna_count++;
		}
		closedir DNA_DIR;
		if( $dna_count == 0 ){
			Tkx::tk___messageBox(
			    -message => "There are no DNA files in the DNA directory '$vPhyloMM->{DNA_Directory}'.",
			    -icon => "error",
			    -title => "Error...",
			);
			return -1;
		}
	}
	if( $count && !$vPhyloMM->{Report_SlidingMinPD} && !$transitions ){
		Tkx::tk___messageBox(
			    -message => "The 'Sliding MinPD/transitions' directory is empty and so the selected reports cannot be generated. (Try selecting 'Run Sliding MinPD' along with your other reports.)",
			    -icon => "error",
			    -title => "Error...",
		);
		return -1;
	}
	$count += $dna_count;
	$self->{Progress_Bar}->configure(-maximum=>$count, -mode=>"determinate");
	my $step_size = $count / 100;
	$self->{Refresh_Function} = sub { 
		$self->{Progress_Bar}->step();
		$self->{Report_Status} = sprintf("%.1f%%", (($self->{Progress_Bar}->cget( -value ) / $count ) * 100.0) );
		&Tkx::update;
		return $self->{Cancel};
	};

	if( $vPhyloMM->{Report_SlidingMinPD} ){
		$vPhyloMM->run_slidingminpd_serially(
			Refresh_Function => $self->{Refresh_Function},
		) and do {
			Tkx::tk___messageBox(
			    -message => "Sliding MinPD did not run successfully. Check the log file.",
			    -icon => "error",
			    -title => "Error...",
			);
			return -1;
		};
		return -1 if &{$self->{Refresh_Function}};
	}
	if( $vPhyloMM->{Report_Transitions} ){
		$vPhyloMM->write_transitions(
			Output => File::Spec->catfile( $vPhyloMM->{Output_Directory}, "transitions.csv" ),
		) and do {
			Tkx::tk___messageBox(
			    -message => "The 'transitions' report was not generated successfully. Check the log file.",
			    -icon => "error",
			    -title => "Error...",
			);
			return -1;
		};
		return -1 if &{$self->{Refresh_Function}};
	}
	if( $vPhyloMM->{Report_Aggregates} ){
		$vPhyloMM->write_aggregates(
			Output => File::Spec->catfile( $vPhyloMM->{Output_Directory}, "aggregates.csv" ),
		) and do {
			Tkx::tk___messageBox(
			    -message => "The 'aggregates' report was not generated successfully. Check the log file.",
			    -icon => "error",
			    -title => "Error...",
			);
			return -1;
		};
		return -1 if &{$self->{Refresh_Function}};
	}
	if( $vPhyloMM->{Report_Summary} ){
		$vPhyloMM->write_summary(
			Output => File::Spec->catfile( $vPhyloMM->{Output_Directory}, "summary.csv" ),
		) and do {
			Tkx::tk___messageBox(
			    -message => "The 'summary' report was not generated successfully. Check the log file.",
			    -icon => "error",
			    -title => "Error...",
			);
			return -1;
		};
		return -1 if &{$self->{Refresh_Function}};
	}
	if( $vPhyloMM->{Report_R} ){
		$vPhyloMM->write_r(
			Output => File::Spec->catfile( $vPhyloMM->{Output_Directory}, "mm.csv" ),
		) and do {
			Tkx::tk___messageBox(
			    -message => "The 'Markov model' report was not generated successfully. Check the log file.",
			    -icon => "error",
			    -title => "Error...",
			);
			return -1;
		};
		return -1 if &{$self->{Refresh_Function}};
	}
	if( $vPhyloMM->{Report_Shortest_Paths} ){
		$vPhyloMM->write_shortest_paths(
			Shortest_Path_Directory => $vPhyloMM->{Shortest_Paths_Directory},
		) and do {
			Tkx::tk___messageBox(
			    -message => "The 'shortest paths' report was not generated successfully. Check the log file.",
			    -icon => "error",
			    -title => "Error...",
			);
			return -1;
		};
		return -1 if &{$self->{Refresh_Function}};
	}
	
	if( $vPhyloMM->{Report_Graphs} ){
		$vPhyloMM->write_probability_graphs(
			Graph_Directory => $vPhyloMM->{Graph_Directory},
		) and do {
			Tkx::tk___messageBox(
			    -message => "The 'pathway graphs' report was not generated successfully. Check the log file.",
			    -icon => "error",
			    -title => "Error...",
			);
			return -1;
		};
		return -1 if &{$self->{Refresh_Function}};
	}
	if( $vPhyloMM->{Report_Pruned_R} ){
		$vPhyloMM->write_pruned_r(
			Output => File::Spec->catfile( $vPhyloMM->{Pruned_Directory}, "pruned mm.csv" ),
		) and do {
			Tkx::tk___messageBox(
			    -message => "The 'pruned Markov model' report was not generated successfully. Check the log file.",
			    -icon => "error",
			    -title => "Error...",
			);
			return -1;
		};
		return -1 if &{$self->{Refresh_Function}};
	}
	if( $vPhyloMM->{Report_Pruned_Shortest_Paths} ){
		$vPhyloMM->write_pruned_shortest_paths(
			Pruned_Shortest_Path_Directory => $vPhyloMM->{Pruned_Shortest_Path_Directory},
		) and do {
			Tkx::tk___messageBox(
			    -message => "The 'pruned shortest paths' report was not generated successfully. Check the log file.",
			    -icon => "error",
			    -title => "Error...",
			);
			return -1;
		};
		return -1 if &{$self->{Refresh_Function}};
	}
	if( $vPhyloMM->{Report_Pruned_Graphs} ){
		$vPhyloMM->write_pruned_probability_graphs(
			Pruned_Graphs_Directory => $vPhyloMM->{Pruned_Graphs_Directory},
		) and do {
			Tkx::tk___messageBox(
			    -message => "The 'pruned pathways graphs' report was not generated successfully. Check the log file.",
			    -icon => "error",
			    -title => "Error...",
			);
			return -1;
		};
		return -1 if &{$self->{Refresh_Function}};
	}
	$self->{Progress_Bar}->configure( -maximum => 100, -value => 100 );
	Tkx::update();
	$self->{Report_Status} = "100%";
	Tkx::tk___messageBox(
		-message => "All reports have been generated successfully.",
		-icon => "info",
		-title => "Success...",
	);
	#say "something";
	return 0;
};

sub exit_program{
	my $self = shift;
	if( exists( $self->{Saved} ) && $self->{Saved} != 1 ){
		my $response = Tkx::tk___messageBox(
			-type => "yesnocancel",
			-message => "There may be unsaved changed. Are you sure you'd like quit?",
			-icon => "question",
			-title => "discard changes?"
		);
		if( $response ne 'yes'){
			return 0;				
		}
	}
	Tkx::destroy( $self->{Main_Window} );
	return 0;
}

1;

__END__
