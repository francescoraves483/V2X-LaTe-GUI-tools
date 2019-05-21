#include <stdlib.h>
#include <arpa/inet.h>
#include <errno.h>
#include <gtk/gtk.h>

#define WIDTH 400
#define HEIGHT 100
#define STRSIZE_MAX 255
#define DEFAULT_IP "192.168.1.1" // Default IP address

/* Initializers and total number of buttons */
#define TOTBTN 7 // Total number of buttons
#define BUTTON_LABELS_INITIALIZER {"172 (5.86 GHz)","174 (5.87 GHz)","176 (5.88 GHz)","178 (5.89 GHz)","180 (5.90 GHz)","182 (5.91 GHz)","184 (5.92 GHz)"} // Defined an "initializer" to more easily extend the program and add more buttons

#define INFOTEXT_INITIALIZER { \
	"<b><span font=\"40\" foreground=\"#99CCFF\">172\nSCH\n5.86 GHz</span></b>", \
	"<b><span font=\"40\" foreground=\"#FF4E4E\">174\nSCH\n5.87 GHz</span></b>", \
	"<b><span font=\"40\" foreground=\"#D84747\">176\nSCH\n5.88 GHz</span></b>", \
	"<b><span font=\"40\" foreground=\"#40E24B\">178\nCCH\n5.89 GHz</span></b>", \
	"<b><span font=\"40\" foreground=\"#FF6633\">180\nSCH\n5.90 GHz</span></b>", \
	"<b><span font=\"40\" foreground=\"#DB5B2D\">182\nSCH\n5.91 GHz</span></b>", \
	"<b><span font=\"40\" foreground=\"#9B2DDB\">184\nSCH\n5.92 GHz</span></b>" \
	}

#define IW_FREQ_INITIALIZER {5860,5870,5880,5890,5900,5910,5920}

// Macro to create and run an error message dialog
#define GTK_ERROR_MESSAGE_RUN(errmsg,window,text) errmsg=gtk_message_dialog_new(window,GTK_DIALOG_DESTROY_WITH_PARENT,GTK_MESSAGE_ERROR,GTK_BUTTONS_CLOSE,text); \
													gtk_dialog_run(GTK_DIALOG(errmsg));

static int curr_btnidx; // Index of the current button (used to identify the last correctly set channel)
static int loop_end_errcode=0; // Error code for the last GTK loop iteration. Set to 1 in case the GTK callback encountered an error (are there better solutions than a global variable?)
static char *infoText[]=INFOTEXT_INITIALIZER; // Text to be displayed for each selected channel

// Data structures containing all the parameters to be passed to the GTK callback function (i.e. ssh_to_APU())
struct common_widgets {
    GtkWidget *textinfo_area;
    GtkWidget *info_label;
    GtkEntry *ip_entry, *username_entry, *password_entry;
};

struct gpointer_data {
    struct common_widgets *common_gtk_ptr;
    int btnidx;
};

// Function to create a frame, with label "frame_label", and a text entry inside, with default text "default_text" and visibility specified by "visibility"
// visibility=TRUE -> normal text entry
// visibility=FALSE -> hidden text entry (password field)
static GtkEntry* gtk_entry_in_frame_create(GtkBox *box, const char *frame_label, const char *default_text, gboolean visibility) {
	GtkEntry *entry;
	GtkWidget *frame;

	frame=gtk_frame_new(frame_label);
    gtk_box_pack_start(box,frame,TRUE,TRUE,2);

    entry=GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_text(entry,default_text);
    gtk_entry_set_visibility(entry,visibility);
    gtk_container_add(GTK_CONTAINER(frame),GTK_WIDGET(entry));

	return entry;
}

// GTK callback function
static void ssh_to_APU(GtkWidget *widget, gpointer data) {
	struct gpointer_data *data_struct=data;
	int freqarray[]=IW_FREQ_INITIALIZER;
	char labelstr[STRSIZE_MAX];
	char cmdstr[STRSIZE_MAX];
	struct in_addr parsedIPaddr;
	long pw_len;
	GtkWidget *dialog;

	// Get password length (if it is 0, no password was specified)
	pw_len=strlen(gtk_entry_get_text(data_struct->common_gtk_ptr->password_entry));

	// Check, if a password is specified, whether sshpass is available
	if(system("dpkg -s sshpass >/dev/null 2>&1") && pw_len>0) {
		loop_end_errcode=1;
   		gtk_main_quit();
   	} else {
	   	// Parse IP address
		if(!inet_pton(AF_INET,gtk_entry_get_text(data_struct->common_gtk_ptr->ip_entry),(struct in_addr *)&parsedIPaddr)) {
			gtk_label_set_text(GTK_LABEL(data_struct->common_gtk_ptr->info_label),"Invalid IP address specified!");
		} else {
			if(data_struct->btnidx==curr_btnidx) {
				// If the current buttons corresponds to the last correctly set channel, do nothing and print a warning to the user, using the info_label label
				gtk_label_set_text(GTK_LABEL(data_struct->common_gtk_ptr->info_label),"Channel already selected.");
			} else {
				// Parse command (if a password is specified, use sshpass, otheriwe, directly use ssh)
				if(pw_len) {
					snprintf(cmdstr,STRSIZE_MAX,"sshpass -p %s ssh -o StrictHostKeyChecking=no -o ConnectTimeout=2 %s@%s \"iw dev wlan0 ocb leave && iw dev wlan0 ocb join %d 10MHz\" >/dev/null 2>&1",
						gtk_entry_get_text(data_struct->common_gtk_ptr->password_entry),
						gtk_entry_get_text(data_struct->common_gtk_ptr->username_entry),
						inet_ntoa(parsedIPaddr),
						freqarray[data_struct->btnidx]);
				} else {
					snprintf(cmdstr,STRSIZE_MAX,"ssh -o StrictHostKeyChecking=no -o ConnectTimeout=2 %s@%s \"iw dev wlan0 ocb leave && iw dev wlan0 ocb join %d 10MHz\" >/dev/null 2>&1",
						gtk_entry_get_text(data_struct->common_gtk_ptr->username_entry),
						inet_ntoa(parsedIPaddr),
						freqarray[data_struct->btnidx]);
				}

				// Print "Connecting..." while trying to connect with ssh
				gtk_label_set_text(GTK_LABEL(data_struct->common_gtk_ptr->info_label),"Connecting...");
				// Update the label right now, not when the callback function returns
				while(gtk_events_pending()) {
	 				gtk_main_iteration();
				}

				// Set channel using system() and ssh (using iw ocb leave && ocb join)
				if(!system(cmdstr)) {
					// Update text information about the current channel
					gtk_label_set_markup(GTK_LABEL(data_struct->common_gtk_ptr->textinfo_area),infoText[data_struct->btnidx]);

					// Set info
					snprintf(labelstr,STRSIZE_MAX,"SSH to %s@%s done.",gtk_entry_get_text(data_struct->common_gtk_ptr->username_entry),inet_ntoa(parsedIPaddr));
					gtk_label_set_text(GTK_LABEL(data_struct->common_gtk_ptr->info_label),labelstr);

					// Set this channel as the last one which was correctly set
					curr_btnidx=data_struct->btnidx;
				} else {
					// Set info if an error occurred
					snprintf(labelstr,STRSIZE_MAX,"SSH to %s@%s failed.",gtk_entry_get_text(data_struct->common_gtk_ptr->username_entry),inet_ntoa(parsedIPaddr));
					gtk_label_set_text(GTK_LABEL(data_struct->common_gtk_ptr->info_label),labelstr);
				}
			}
		}
	}
}

int main(int argc, char *argv[]) {
    GtkWidget *main_window, *textinfo_area;
    GtkWidget *textinfo_frame, *buttons_frame, *info_frame;
    GtkWidget *button_box, *buttons[TOTBTN];
    GtkWidget *main_box;
    GtkWidget *info_label;
    GtkEntry *ip_entry, *username_entry, *password_entry;
    GtkWidget *errormsg; // GTK dialog for error messages
    struct common_widgets common_gtk;
    struct gpointer_data data[TOTBTN];
    char *buttonLabels[]=BUTTON_LABELS_INITIALIZER;
    int btnidx;
    unsigned long default_idx; // Default index read from command line -> it should match the first channel which is set on the APU boards, among the available ones (typ. CCH)

    if(argc!=2) {
    	fprintf(stderr,"Error. One argument should be specified.\n");
    	fprintf(stdout,"Usage:\n"
    		"%s <default_index_number (between 0 and %d)>",argv[0],TOTBTN);

    	fprintf(stdout,"\nValid indeces:\n");
    	fprintf(stdout,"\t[Index]\t[Channel]\n");
    	for(btnidx=0;btnidx<TOTBTN;btnidx++) {
    		fprintf(stdout,"\t%d\t%s\n",btnidx,buttonLabels[btnidx]);
    	}

    	exit(EXIT_FAILURE);
    }

    // Parse default index
    errno=0;
    default_idx=strtoul(argv[1],NULL,0);
    if(errno || default_idx>=TOTBTN) {
    	fprintf(stderr,"Invalid default index. The program will be terminated now.\n");
    	exit(EXIT_FAILURE);
    }

    // gtk_init() shall be launched at startup
    gtk_init(0,NULL);

    // Create a new GTK Window
    main_window=gtk_window_new(GTK_WINDOW_TOPLEVEL);

    // Set the gtk_main_quit() events when closing the window
    g_signal_connect(G_OBJECT(main_window),"destroy",gtk_main_quit, NULL);
    g_signal_connect(G_OBJECT(main_window),"delete-event",gtk_main_quit, NULL);
    
    // Set window title
    gtk_window_set_title(GTK_WINDOW(main_window),"APU channel selector");

    // Set window size and make it non-resizable
    gtk_widget_set_size_request(main_window,WIDTH,HEIGHT);
    gtk_window_set_resizable(GTK_WINDOW(main_window),FALSE);

    // Set border width
    gtk_container_set_border_width(GTK_CONTAINER(main_window),10);

    // Create a new GtkBox where all the other widgets will be inserted
    main_box=gtk_box_new(GTK_ORIENTATION_VERTICAL,2);
    gtk_container_add(GTK_CONTAINER(main_window),main_box);

    // Create a text info frame where the information about the current channel will be displayed
    textinfo_frame=gtk_frame_new("Current channel: ");
    gtk_box_pack_start(GTK_BOX(main_box),textinfo_frame,TRUE,TRUE,2);

    // Set current button index
    curr_btnidx=default_idx;

    textinfo_area=gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(textinfo_area),infoText[default_idx]);
    gtk_label_set_justify(GTK_LABEL(textinfo_area),GTK_JUSTIFY_CENTER);
    gtk_container_add(GTK_CONTAINER(textinfo_frame),textinfo_area);

    // Create a new frame and button box to show the three buttons
    buttons_frame=gtk_frame_new("Set channel: ");
    // When dealing with GtkBox, use gtk_box_pack_start() instead of gtk_container_add() - see: https://developer.gnome.org/gtk3/stable/GtkBox.html#gtk-box-pack-start
    gtk_box_pack_start(GTK_BOX(main_box),buttons_frame,TRUE,TRUE,2);

    button_box=gtk_button_box_new(GTK_ORIENTATION_VERTICAL);
    gtk_container_add(GTK_CONTAINER(buttons_frame),button_box);

    // Create three frames + text entries for IP address, username and password
    ip_entry=gtk_entry_in_frame_create(GTK_BOX(main_box),"SSH IP address: ",DEFAULT_IP,TRUE);
    username_entry=gtk_entry_in_frame_create(GTK_BOX(main_box),"SSH username: ","root",TRUE);
    password_entry=gtk_entry_in_frame_create(GTK_BOX(main_box),"SSH password: ","",FALSE);

    // Create information frame and label, where short text information will be shown
    info_frame=gtk_frame_new("Info: ");
    gtk_box_pack_start(GTK_BOX(main_box),info_frame,TRUE,TRUE,2);

    info_label=gtk_label_new("Program started.");
    gtk_container_add(GTK_CONTAINER(info_frame),info_label);

    // Fill in the common widgets structure
    common_gtk.textinfo_area=textinfo_area;
    common_gtk.info_label=info_label;
    common_gtk.ip_entry=ip_entry;
    common_gtk.username_entry=username_entry;
    common_gtk.password_entry=password_entry;
    
    // Create the three buttons, using the labels sequentially specified in the "buttonLabels" array
    for(btnidx=0;btnidx<TOTBTN;btnidx++) {
    	buttons[btnidx]=gtk_button_new_with_label(buttonLabels[btnidx]);

	    // Fill in the data structure
	    data[btnidx].common_gtk_ptr=&common_gtk;
	    data[btnidx].btnidx=btnidx;

	    // Set the callback function (ssh_to_APU()) when the current button is clicked
	    g_signal_connect(buttons[btnidx],"clicked",G_CALLBACK(ssh_to_APU),&(data[btnidx]));

	    // Add the current button to the GtkButtonBox widget
	    gtk_container_add(GTK_CONTAINER(button_box),buttons[btnidx]);
	}

	// Show the main_window widget and all its children (i.e. all the widgets)
    gtk_widget_show_all(main_window);

    // If openssh is not available, print an error message and exit
    if(system("dpkg -s openssh-client >/dev/null 2>&1")) {
    	GTK_ERROR_MESSAGE_RUN(errormsg,GTK_WINDOW(main_window),"openssh-client is not installed. Please install it.\n");
		gtk_widget_destroy(errormsg);
    	exit(EXIT_FAILURE);
    }

    // "Runs the main loop until gtk_main_quit() is called" (from the official GTK documentation)
    gtk_main();

    // If an error was encountered, display the proper error message
    if(loop_end_errcode) {
    	GTK_ERROR_MESSAGE_RUN(errormsg,GTK_WINDOW(main_window),"Error: sshpass is not installed.\nWhen a password is specified, it is required.");
		gtk_widget_destroy(errormsg);
    }

    return loop_end_errcode;
}