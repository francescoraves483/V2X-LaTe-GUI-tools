#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <errno.h>
#include <locale.h>
#include <gtk/gtk.h>
#include <slope/slope.h>

#define WIDTH 400 // Window width
#define HEIGHT 100 // Window height
#define STRSIZE_MAX 1024
#define MAX_ARRAYSIZE 120 // Maximum size of the array storing the samples to be plotted - also defined the maximum value for "-t" when calling "iperf -c"
#define NUMSERIES 4 // Total number of series to be displayed in the plot (a change in this value requires to update COLOR_ARRAY_INITIALIZER and SLOPE_LAYOUT_ARRAY_INITIALIZER)
#define DEFAULT_IP "192.168.1.182" // Default IP address
#define DEFAULT_CONNTO_IP "10.10.6.103" // Default IP address to connect to

/* Initializers and total number of buttons */
#define TOTBTN_CLIENT 4 // Total number of client-related buttons
#define TOTBTN_SERVER 3 // Total number of server-related buttons (e.g. check, start, stop, ...)
#define CLIENT_BUTTON_LABELS_INITIALIZER {"BK","BE","VI","VO"} // Defined few "initializers" to more easily extend the program and add more buttons if needed
#define SERVER_BUTTON_LABELS_INITIALIZER {"Check server status","Start server","Kill server"} 
#define FILENAMES_INITIALIZER {"BK.png","BE.png","VI.png","VO.png"}
#define ONOFF_FILENAMES_INITIALIZER {"ON.png","OFF.png","notest.png"}
#define AC_INITIALIZER {"BK","BE","VI","VO"} 
#define COLOR_ARRAY_INITIALIZER {"red","blue","yellow","green"}
#define SLOPE_LAYOUT_ARRAY_INITIALIZER {"r-o","b-o","y-o","g-o"}

#define SRVCMD_CHECK 0
#define SRVCMD_START 1
#define SRVCMD_KILL 2

// Port combination strings and defines
#define FIRST_PORTSTR "server: 47001, client -> 46001"
#define SECOND_PORTSTR "server: 46001, client -> 47001"
#define SRV_PORT_1 47001
#define CLI_PORT_1 46001
#define SRV_PORT_2 46001
#define CLI_PORT_2 47001

// Macro to create and run an error message dialog
#define GTK_ERROR_MESSAGE_RUN(errmsg,window,text) errmsg=gtk_message_dialog_new(window,GTK_DIALOG_DESTROY_WITH_PARENT,GTK_MESSAGE_ERROR,GTK_BUTTONS_CLOSE,text); \
													gtk_dialog_run(GTK_DIALOG(errmsg));

typedef enum {
	CLIENT,
	SERVER
} callmode;

typedef enum {
	ON,
	OFF,
	UNKNOWN
} status;

typedef struct _GtkSpinButtonStruct {
	GtkSpinButton *lValue, *bValue, *tValue;
} GtkSpinButtonStruct;

// These two structures has been created as an alternative to global or static variables, to store data which
//    should be kept between different callback invocations.
struct static_data_btn_callback {
	struct in_addr currentIP;
	status serverStatus;
};

struct static_data_server_parser {
	int prev_total_datagrams;
	int counter;
	int seriescnt;
	double min_goodput;
	double max_goodput;
};

// GTK Widgets related to information labels, uodated by the IO Channel callback
struct label_data {
	GtkWidget *gput_info_label;
	GtkWidget *final_gput_info_label;
	GtkWidget *conn_stab_info_label;
	GtkWidget *color_info_label;
};

// GTK pointers and arrays which are passed to the IO Channel callback in order to update the plot
struct plot_global_data {
	GtkWidget *view;
	SlopeItem *series[NUMSERIES];
	double **currDataPoints;
	double **currTimePoints;
};

// Data which is passed to the server parser, through "gpointer data" -> included the previous two structures
struct parser_data {
	struct label_data label_pointers;
	struct plot_global_data plot_pointers;
	struct static_data_server_parser *static_vars_parse_ptr;
};

// Data, including pointers to widgets which should be updated, which must be passed to the button callback (i.e. ssh_to_APU()) through "common_gtk_ptr"
// This data is common, in the sense that it is used by ssh_to_APU() every time, no matter which button is pressed.
struct common_data {
	GtkWidget *info_label;
	GtkEntry *ip_entry, *connto_ip_entry, *username_entry, *password_entry;
	GtkComboBoxText *clientserver_port_entry;
	GtkSpinButtonStruct spin_buttons;

	struct parser_data parser_pointers;

	struct static_data_btn_callback *static_vars_btn_ptr;
};

// Data structure containing all the parameters to be passed to the GTK callback function (i.e. ssh_to_APU()).
// An array of struct gpointer_data, for both the client and server-related buttons, with a number of elements equal to the number of buttons, is declared,
//    in order to pass the proper array element, with the proper button specific data set, when each button is clicked.
//    This workaround allowed to create a single callback function.
struct gpointer_data {
	int btnidx; // Button specific data
	callmode callmode; // Button specific data
	GtkWidget *img_area_ptr; // Button specific data

	struct common_data *common_gtk_ptr; // Button common data (pointer)
};

static int loop_end_errcode=0; // Error code for the last GTK loop iteration. Set to >0 in case the GTK callback encountered an error (are there better solutions than a global variable?)
static char *fileNames[]=FILENAMES_INITIALIZER; // Name of the .png images to be shown for each AC
static char *onOffFileNames[]=ONOFF_FILENAMES_INITIALIZER; // Name of the .png images to be shown for the server being on/off

// Non-callback function prototypes
static gboolean serverParser(GIOChannel *source, GIOCondition condition, gpointer data);
static void ssh_to_APU(GtkWidget *widget, gpointer data);
static GtkEntry* gtk_entry_in_frame_create(GtkBox *box, const char *frame_label, const char *default_text, gboolean visibility);
static GtkComboBoxText* gtk_combobox_in_frame_create(GtkBox *box, const char *frame_label, const char **elements, unsigned int elements_no, gint active_idx);
static void staticDataStructInit(struct static_data_btn_callback *static_vars_btn_ptr,struct static_data_server_parser *static_vars_parse_ptr);

// Initializes the data structures which are accessed by the callback to store data between different iterations
static void staticDataStructInit(struct static_data_btn_callback *static_vars_btn_ptr,struct static_data_server_parser *static_vars_parse_ptr) {
	static_vars_btn_ptr->currentIP.s_addr=0;
	static_vars_btn_ptr->serverStatus=UNKNOWN;

	static_vars_parse_ptr->prev_total_datagrams=-1;
	static_vars_parse_ptr->counter=0;
	static_vars_parse_ptr->seriescnt=0;
	static_vars_parse_ptr->min_goodput=DBL_MAX;
	static_vars_parse_ptr->max_goodput=-1.0;
}

// Callback for the IO Channel, connected to the iPerf server. It is called every time the server outputs new data, in order to parse them.
static gboolean serverParser(GIOChannel *source, GIOCondition condition, gpointer data) {
	GIOStatus opstatus;
	gchar *linebuf; 
	gsize strsize_linebuf;

	double goodput, final_goodput;
	char unit_letter;
	int total_datagrams;
	char first_char;
	float curr_sec,final_sec;

	struct parser_data *parser_data_struct=data;

	gchar *gput_label_str=NULL, *final_gput_label_str=NULL, *conn_stab_label_str=NULL, *color_info_label_str=NULL;

	int scan_retval=0;
	char *colorArray[]=COLOR_ARRAY_INITIALIZER;

	g_print("[DEBUG] Server parser started\n");

	opstatus=g_io_channel_read_line(source,&linebuf,&strsize_linebuf,NULL,NULL);

	if(opstatus==G_IO_STATUS_NORMAL && strsize_linebuf!=0) {
		// Parse useful data
		scan_retval=sscanf(linebuf,"%c %*s %f%*[- *]%f %*s %*f %*s %lf %c%*s %*f %*s %*f%*[/ *]%d %*s",&first_char,&curr_sec,&final_sec,&goodput,&unit_letter,&total_datagrams);

		// Check if the current line is useful. If it is not, terminate the current callback execution.
		if(first_char!='[' || unit_letter!='M' || scan_retval!=6) {
			g_print("[DEBUG] Server parser terminated: useless line\n");
			return TRUE;
		}

		g_print("[DEBUG] Line number = %d | line = %s\n",parser_data_struct->static_vars_parse_ptr->counter,linebuf);

		g_print("[DEBUG] Line number = %d | first char = %c\n",parser_data_struct->static_vars_parse_ptr->counter,first_char);
		g_print("[DEBUG] Line number = %d | sec = %.1f-%.1f\n",parser_data_struct->static_vars_parse_ptr->counter,curr_sec,final_sec);
		g_print("[DEBUG] Line number = %d | goodput value = %lf\n",parser_data_struct->static_vars_parse_ptr->counter,goodput);
		g_print("[DEBUG] Line number = %d | unit letter = %c\n",parser_data_struct->static_vars_parse_ptr->counter,unit_letter);
		g_print("[DEBUG] Line number = %d | total packets = %d | prev value = %d\n",parser_data_struct->static_vars_parse_ptr->counter,total_datagrams,parser_data_struct->static_vars_parse_ptr->prev_total_datagrams);

		// Is this the first line we are reading? If yes (i.e. if prev_total_datagrams is still -1), set the information panel accordingly
		if(parser_data_struct->static_vars_parse_ptr->prev_total_datagrams==-1) {
			gtk_label_set_markup(GTK_LABEL(parser_data_struct->label_pointers.final_gput_info_label),"<b><span font=\"70\" foreground=\"blue\">-</span></b>");
			gtk_label_set_markup(GTK_LABEL(parser_data_struct->label_pointers.conn_stab_info_label),"<b><span font=\"70\" foreground=\"blue\">-</span></b>");

			color_info_label_str=g_strdup_printf("<span foreground=\"%s\">Current plot: %s.</span>",colorArray[parser_data_struct->static_vars_parse_ptr->seriescnt],colorArray[parser_data_struct->static_vars_parse_ptr->seriescnt]);
			gtk_label_set_markup(GTK_LABEL(parser_data_struct->label_pointers.color_info_label),color_info_label_str);

			g_free(color_info_label_str);
		}

		// Check then if this is the last line generated by iPerf, trying to discrimate the case in which a stuck client generates only one line
		// Typically, when only one line is generated, the final_sec has a decimal digit different than 0, due to a increased/reduced overall
		//    test duration. This is used as a rule of thumb to detect such a situation, which may happen for instance when an interfering
		//    VO traffic is blocking a BE or BK one.
		// The order condition detects the last line as a line starting with '0.0-', excluding the first line by checking prev_total_datagrams!=-1 and
		//    total_datagrams>prev_total_datagrams.
		// If this is the last line, update the connection stability and average goodput labels in the info panel on the right.
		if((parser_data_struct->static_vars_parse_ptr->prev_total_datagrams!=-1 && total_datagrams>parser_data_struct->static_vars_parse_ptr->prev_total_datagrams && curr_sec==0.0) || (curr_sec==0.0 && (int)(final_sec*10)%10!=0)) {
			final_goodput=goodput;

			final_gput_label_str=g_strdup_printf("<b><span font=\"70\" foreground=\"blue\">%.2f</span></b>",goodput);
			gtk_label_set_markup(GTK_LABEL(parser_data_struct->label_pointers.final_gput_info_label),final_gput_label_str);

			if(parser_data_struct->static_vars_parse_ptr->min_goodput!=DBL_MAX) {
				conn_stab_label_str=g_strdup_printf("<b><span font=\"70\" foreground=\"blue\">%.3lf %%</span></b>",(parser_data_struct->static_vars_parse_ptr->max_goodput-parser_data_struct->static_vars_parse_ptr->min_goodput)*100/parser_data_struct->static_vars_parse_ptr->max_goodput);
			} else {
				conn_stab_label_str=g_strdup_printf("<b><span font=\"70\" foreground=\"blue\">n.d. %%</span></b>");
			}
			gtk_label_set_markup(GTK_LABEL(parser_data_struct->label_pointers.conn_stab_info_label),conn_stab_label_str);

			gtk_label_set_markup(GTK_LABEL(parser_data_struct->label_pointers.gput_info_label),"<b><span font=\"70\" foreground=\"#018729\">-</span></b>");

			parser_data_struct->static_vars_parse_ptr->prev_total_datagrams=-1;
			parser_data_struct->static_vars_parse_ptr->min_goodput=DBL_MAX;
			parser_data_struct->static_vars_parse_ptr->max_goodput=-1.0;

  			parser_data_struct->static_vars_parse_ptr->counter=0;
  			parser_data_struct->static_vars_parse_ptr->seriescnt=(parser_data_struct->static_vars_parse_ptr->seriescnt+1)%NUMSERIES;
		} else {
			// If this is not the last line, compute the current minimum/maximum reported "bandwidth" (goodput) value, needed for the connection stability computation
			if(goodput<parser_data_struct->static_vars_parse_ptr->min_goodput) {
				parser_data_struct->static_vars_parse_ptr->min_goodput=goodput;
			}

			if(goodput>parser_data_struct->static_vars_parse_ptr->max_goodput) {
				parser_data_struct->static_vars_parse_ptr->max_goodput=goodput;
			}

			parser_data_struct->static_vars_parse_ptr->prev_total_datagrams=total_datagrams;
			gput_label_str=g_strdup_printf("<b><span font=\"70\" foreground=\"#018729\">%.2f</span></b>",goodput);
			gtk_label_set_markup(GTK_LABEL(parser_data_struct->label_pointers.gput_info_label),gput_label_str);

			// Store the current points in the x and y data arrays to be plotted
			parser_data_struct->plot_pointers.currDataPoints[parser_data_struct->static_vars_parse_ptr->seriescnt][parser_data_struct->static_vars_parse_ptr->counter]=goodput;
			parser_data_struct->plot_pointers.currTimePoints[parser_data_struct->static_vars_parse_ptr->seriescnt][parser_data_struct->static_vars_parse_ptr->counter]=curr_sec;
			parser_data_struct->static_vars_parse_ptr->counter++;

			// Update plot
			slope_xyseries_update_data(SLOPE_XYSERIES(parser_data_struct->plot_pointers.series[parser_data_struct->static_vars_parse_ptr->seriescnt]),parser_data_struct->plot_pointers.currTimePoints[parser_data_struct->static_vars_parse_ptr->seriescnt],parser_data_struct->plot_pointers.currDataPoints[parser_data_struct->static_vars_parse_ptr->seriescnt],parser_data_struct->static_vars_parse_ptr->counter);

  			slope_view_redraw(SLOPE_VIEW(parser_data_struct->plot_pointers.view));
		}
	}

	if(gput_label_str) g_free(gput_label_str);
	if(final_gput_label_str) g_free(final_gput_label_str);
	g_free(linebuf);

	g_print("[DEBUG] Server parser terminated: useful line\n");

	return TRUE;
}

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

// Function to create a frame, with label "frame_label", and a combo box inside, with "elements_no" elements specified in "elements" and an active default
// element specified with "active_idx"
static GtkComboBoxText* gtk_combobox_in_frame_create(GtkBox *box, const char *frame_label, const char **elements, unsigned int elements_no, gint active_idx) {
	GtkComboBoxText *combobox;
	GtkWidget *frame;

	frame=gtk_frame_new(frame_label);
    gtk_box_pack_start(box,frame,TRUE,TRUE,2);

    combobox=GTK_COMBO_BOX_TEXT(gtk_combo_box_text_new());
   	for (int i=0; i<elements_no; i++) {
  		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combobox),elements[i]);
  	}
    gtk_combo_box_set_active(GTK_COMBO_BOX(combobox),active_idx);
    gtk_container_add(GTK_CONTAINER(frame),GTK_WIDGET(combobox));

	return combobox;
}


// GTK callback function
static void ssh_to_APU(GtkWidget *widget, gpointer data) {
	struct gpointer_data *data_struct=data;
	char *acarray[]=AC_INITIALIZER;

	gchar *labelstr=NULL;
	char cmdstr[STRSIZE_MAX];

	struct in_addr parsedIPaddr, parsedCONNTOIPaddr;

	long pw_len;
	int serverport, clientport;
	gchar *port_str;

	int sys_retval;
	int str_char_count=0;

	GtkWidget *dialog;
	GIOChannel *iperfIOchannel;

	FILE *iperfFp=NULL;
	int iperfFd;

	// Get password length (if it is 0, no password was specified)
	pw_len=strlen(gtk_entry_get_text(data_struct->common_gtk_ptr->password_entry));

	// Check, if a password is specified, whether sshpass is available
	if(system("dpkg -s sshpass >/dev/null 2>&1") && pw_len>0) {
		loop_end_errcode=1;
   		gtk_main_quit();
   	}

	if(pw_len) {
		// If a password is specified, prepend 'sshpass' to the command string
		str_char_count=snprintf(cmdstr,STRSIZE_MAX,"sshpass -p %s ",gtk_entry_get_text(data_struct->common_gtk_ptr->password_entry));
	}

	// Parse SSH IP address and IP address to connect to
	if(!inet_pton(AF_INET,gtk_entry_get_text(data_struct->common_gtk_ptr->ip_entry),(struct in_addr *)&parsedIPaddr)) {
		gtk_label_set_text(GTK_LABEL(data_struct->common_gtk_ptr->info_label),"Error:\nInvalid IP address specified!");
	} else if(!inet_pton(AF_INET,gtk_entry_get_text(data_struct->common_gtk_ptr->connto_ip_entry),(struct in_addr *)&parsedCONNTOIPaddr)) {
		gtk_label_set_text(GTK_LABEL(data_struct->common_gtk_ptr->info_label),"Error:\nInvalid destination IP address specified!");
	} else {
		// Make the user check the server status before doing other operations (this may be needed for proper GUI operations)
		if(!(data_struct->callmode==SERVER && data_struct->btnidx==SRVCMD_CHECK) && (data_struct->common_gtk_ptr->static_vars_btn_ptr->serverStatus==UNKNOWN || data_struct->common_gtk_ptr->static_vars_btn_ptr->currentIP.s_addr!=parsedIPaddr.s_addr)) {
			gtk_label_set_markup(GTK_LABEL(data_struct->common_gtk_ptr->info_label),"<span foreground=\"red\">Error:\nPlease check the server status before going on!</span>\n");
			return;
		}

		// Check if SSH can connect to the target board and if stdbuf (required for the server) is installed
		snprintf(cmdstr+str_char_count,STRSIZE_MAX-str_char_count,"ssh -o StrictHostKeyChecking=no -o ConnectTimeout=2 %s@%s \"opkg list-installed | grep coreutils-stdbuf\" >/dev/null 2>&1",
			gtk_entry_get_text(data_struct->common_gtk_ptr->username_entry),
			inet_ntoa(parsedIPaddr));

		sys_retval=system(cmdstr)>>8; // First sys_retval (>>8 to get the process return value)

		if(sys_retval==255) { // First sys_retval
			gtk_image_set_from_file(GTK_IMAGE(data_struct->img_area_ptr),onOffFileNames[2]);
		} else if(sys_retval==1) {
			loop_end_errcode=2;
			gtk_label_set_markup(GTK_LABEL(data_struct->common_gtk_ptr->info_label),"<span foreground=\"red\">Error:\ncoreutils-stdbuf is not installed in the target system.\nPlease use opkg to install it.</span>\n");
	   		return;
	   	} else {
	   		// ---------------- Prepare the command string ----------------

			// Avoid killing a non-running server or launching two equal servers
			if(data_struct->callmode==SERVER && data_struct->btnidx==SRVCMD_KILL && data_struct->common_gtk_ptr->static_vars_btn_ptr->serverStatus==OFF) {
				gtk_label_set_text(GTK_LABEL(data_struct->common_gtk_ptr->info_label),"No server is running!\n");
				return;
			} else if(data_struct->callmode==SERVER && data_struct->btnidx==SRVCMD_START && data_struct->common_gtk_ptr->static_vars_btn_ptr->serverStatus==ON) {
				gtk_label_set_text(GTK_LABEL(data_struct->common_gtk_ptr->info_label),"A server is already running: only one server can be launched at a time!\n");
				return;
			}

			// Parse port combination
			port_str=gtk_combo_box_text_get_active_text(data_struct->common_gtk_ptr->clientserver_port_entry);
			if(strcmp(port_str,FIRST_PORTSTR)==0) {
				serverport=SRV_PORT_1;
				clientport=CLI_PORT_1;
			} else {
				serverport=SRV_PORT_2;
				clientport=CLI_PORT_2;
			}


			// Start composing the command string by adding the ssh part
			str_char_count+=snprintf(cmdstr+str_char_count,STRSIZE_MAX-str_char_count,"ssh -o StrictHostKeyChecking=no -o ConnectTimeout=2 %s@%s",
				gtk_entry_get_text(data_struct->common_gtk_ptr->username_entry),
				inet_ntoa(parsedIPaddr));

			if(data_struct->callmode==CLIENT) { // CLIENT call mode
				// Before starting any client, check if a client is already in execution
				strncat(cmdstr," \"ps | grep -w 'iperf -c' | grep -v 'grep'\" >/dev/null 2>&1",STRSIZE_MAX-str_char_count);

				if((system(cmdstr)>>8)==0) {
					gtk_label_set_markup(GTK_LABEL(data_struct->common_gtk_ptr->info_label),"<span foreground=\"red\">Error:\nA test is already running.\nPlease wait until it finishes!</span>\n");
					return;
				}

				// Prepare command string for the iPerf client
				snprintf(cmdstr+str_char_count,STRSIZE_MAX-str_char_count," nohup iperf -c %s -i 1 -t %d -l %d -b %dM -u -p %d -A %s >/dev/null 2>&1 &",
					inet_ntoa(parsedCONNTOIPaddr),
					gtk_spin_button_get_value_as_int(data_struct->common_gtk_ptr->spin_buttons.tValue),
					gtk_spin_button_get_value_as_int(data_struct->common_gtk_ptr->spin_buttons.lValue),
					gtk_spin_button_get_value_as_int(data_struct->common_gtk_ptr->spin_buttons.bValue),
					clientport,
					acarray[data_struct->btnidx]);
			} else { // SERVER call mode
				if(data_struct->btnidx==SRVCMD_CHECK) { // Check command -> check if a server is already in execution
					snprintf(cmdstr+str_char_count,STRSIZE_MAX-str_char_count," \"ps | grep -w 'iperf -s' | grep -v 'grep'\" >/dev/null 2>&1");
				} else if(data_struct->btnidx==SRVCMD_START) { // Start command -> start a server and set up the GIOChannel to parse the reported data
					snprintf(cmdstr+str_char_count,STRSIZE_MAX-str_char_count," stdbuf -o L iperf -s -u -i 1 -p %d -f m | stdbuf -o L sed 's/\\s\\s*/ /g'",
						serverport);

					// Start server using popen()
					iperfFp=popen(cmdstr,"r");

					if(!iperfFp) {
						loop_end_errcode=3;
						g_print("[DEBUG] Error in launching the server. errno = %d\n",errno);
						return;
					}

					iperfFd=fileno(iperfFp);

					iperfIOchannel=g_io_channel_unix_new(iperfFd);
					g_io_channel_set_flags(iperfIOchannel,G_IO_FLAG_NONBLOCK,NULL);
					//g_io_channel_set_line_term(iperfIOchannel,NULL,-1);
					g_io_add_watch(iperfIOchannel,G_IO_IN,serverParser,&(data_struct->common_gtk_ptr->parser_pointers));
				} else if(data_struct->btnidx==SRVCMD_KILL) { // Kill command -> kill any server which is in execution
					snprintf(cmdstr+str_char_count,STRSIZE_MAX-str_char_count," killall iperf >/dev/null 2>&1");
				}
			}
		}

		// ---------------- Execute the command string ----------------
		if(sys_retval==0) { // First sys_retval -> sys_retval==0 means "ok, we can connect using SSH"
			// Print "Connecting..." while trying to connect with ssh
			gtk_label_set_text(GTK_LABEL(data_struct->common_gtk_ptr->info_label),"Connecting...");
			// Update the label right now, not when the callback function returns
			while(gtk_events_pending()) {
		 		gtk_main_iteration();
			}

			// Execute command
			if(!(data_struct->callmode==SERVER && data_struct->btnidx==SRVCMD_START)) {
				sys_retval=system(cmdstr); // Second sys_retval
			} else {
				sys_retval=0; // Second sys_retval (when opening the server with popen(), directly set sys_retval to 0, as errors are already detected in the FILE pointer returned by popen())
			}

			sys_retval=sys_retval>>8; // Second sys_retval

			if(data_struct->callmode==SERVER) { // SERVER call mode
				// Update .png image according to the server status, taking into account that the server check command will return
				// '1' if no server is in execution, otherwise '0'
				if(sys_retval==255) { // Second sys_retval
					gtk_image_set_from_file(GTK_IMAGE(data_struct->img_area_ptr),onOffFileNames[2]);
				} else if(sys_retval==1) { // Second sys_retval
					if(data_struct->btnidx==SRVCMD_CHECK) {
						gtk_image_set_from_file(GTK_IMAGE(data_struct->img_area_ptr),onOffFileNames[1]);
						data_struct->common_gtk_ptr->static_vars_btn_ptr->serverStatus=OFF;
					}
					data_struct->common_gtk_ptr->static_vars_btn_ptr->currentIP=parsedIPaddr;
				} else if(sys_retval==0) { // Second sys_retval
					gtk_image_set_from_file(GTK_IMAGE(data_struct->img_area_ptr),onOffFileNames[data_struct->btnidx==SRVCMD_KILL?1:0]);

					switch(data_struct->btnidx) {
						case SRVCMD_KILL:
							data_struct->common_gtk_ptr->static_vars_btn_ptr->serverStatus=OFF;
						break;

						case SRVCMD_START:
							data_struct->common_gtk_ptr->static_vars_btn_ptr->serverStatus=ON;
						break;

						case SRVCMD_CHECK:
							data_struct->common_gtk_ptr->static_vars_btn_ptr->serverStatus=ON;
						break;

						default:
							data_struct->common_gtk_ptr->static_vars_btn_ptr->serverStatus=UNKNOWN;
					}
					data_struct->common_gtk_ptr->static_vars_btn_ptr->currentIP=parsedIPaddr;
				} else {
					g_print("Unexpected SSH return value: %d.\n",sys_retval);
					gtk_image_set_from_file(GTK_IMAGE(data_struct->img_area_ptr),onOffFileNames[2]);
				}


			} else { // CLIENT call mode
				// If everything went well, display the current/last AC used by the client
				if(sys_retval==0) {
					gtk_image_set_from_file(GTK_IMAGE(data_struct->img_area_ptr),fileNames[data_struct->btnidx]);
				}
			}

			if(sys_retval!=255) { // Second sys_retval
				labelstr=g_strdup_printf("SSH to %s@%s succeeded!",gtk_entry_get_text(data_struct->common_gtk_ptr->username_entry),inet_ntoa(parsedIPaddr));
				gtk_label_set_text(GTK_LABEL(data_struct->common_gtk_ptr->info_label),labelstr);
				g_free(labelstr);
			}
		} else if(sys_retval==255) { // First sys_retval
			// Set info if an error occurred in SSH
			labelstr=g_strdup_printf("SSH to %s@%s failed.",gtk_entry_get_text(data_struct->common_gtk_ptr->username_entry),inet_ntoa(parsedIPaddr));
			gtk_label_set_text(GTK_LABEL(data_struct->common_gtk_ptr->info_label),labelstr);
			g_free(labelstr);
		}
	}
}

int main(int argc, char *argv[]) {
    GtkWidget *main_window, *window_box, *img_area, *server_img_area;
    GtkWidget *img_frame, *buttons_frame, *info_frame, *gput_info_frame, *final_gput_info_frame, *conn_stab_info_frame, *server_img_frame, *server_buttons_frame;
    GtkWidget *button_box, *server_button_box, *buttons[TOTBTN_CLIENT], *server_buttons[TOTBTN_CLIENT], *server_check_button, *server_start_button, *server_stop_button;
    GtkWidget *main_box, *info_box;
    GtkWidget *info_label;
    GtkEntry *ip_entry, *username_entry, *password_entry, *connto_ip_entry;
    GtkComboBoxText *clientserver_port_entry;
    GtkWidget *errormsg; // GTK dialog for error messages

    struct gpointer_data client_data[TOTBTN_CLIENT];
    struct gpointer_data server_data[TOTBTN_SERVER];
    struct parser_data parser_data_struct;
    struct common_data common_gtk;

    struct static_data_btn_callback static_vars_btn;
    struct static_data_server_parser static_vars_parse;

    char *clientButtonLabels[]=CLIENT_BUTTON_LABELS_INITIALIZER;
    char *serverButtonLabels[]=SERVER_BUTTON_LABELS_INITIALIZER;
    const gchar *port_labels[]={FIRST_PORTSTR,SECOND_PORTSTR};

    int btnidx, srvbtnidx; // client button index and server button index

	GtkWidget *regulation_frame, *regulation_box;
	GtkAdjustment *lValue, *bValue, *tValue;
	GtkSpinButtonStruct spin_buttons;

	GtkWidget *plot_frame, *plot;
	SlopeScale *scale;

	GtkWidget *color_info_frame;
	SlopeItem *legend;
	SlopeFigure *figure;

	gchar *slopeLayoutArray[]=SLOPE_LAYOUT_ARRAY_INITIALIZER;

    if(argc!=1) {
    	fprintf(stderr,"Error. No arguments should be specified.\n");
    	fprintf(stdout,"Usage:\n"
    		"%s",argv[0]);

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
    gtk_window_set_title(GTK_WINDOW(main_window),"iPerf test launcher");

    // Set window size and make it non-resizable
    gtk_widget_set_size_request(main_window,WIDTH,HEIGHT);
    gtk_window_set_resizable(GTK_WINDOW(main_window),FALSE);

    // Set border width
    gtk_container_set_border_width(GTK_CONTAINER(main_window),10);

    // Create the main window box
   	window_box=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,2);
    gtk_container_add(GTK_CONTAINER(main_window),window_box);

    // Create a new GtkBox where all the other widgets will be inserted, except for server parsed information (placed on the left)
    main_box=gtk_box_new(GTK_ORIENTATION_VERTICAL,2);
    gtk_box_pack_start(GTK_BOX(window_box),main_box,TRUE,TRUE,2);

    // Create image frame and GtkImage to show the AC .png images
    img_frame=gtk_frame_new("Current/Last AC: ");
    gtk_box_pack_start(GTK_BOX(main_box),img_frame,TRUE,TRUE,2);

    img_area=gtk_image_new_from_file(onOffFileNames[2]);
    gtk_container_add(GTK_CONTAINER(img_frame),img_area);

    // Create image frame to display current server status
    server_img_frame=gtk_frame_new("Server status: ");
    gtk_box_pack_start(GTK_BOX(main_box),server_img_frame,TRUE,TRUE,2);

    server_img_area=gtk_image_new_from_file(onOffFileNames[2]);
    gtk_container_add(GTK_CONTAINER(server_img_frame),server_img_area);

    // Create a new frame and button box to show the TOTBTN_CLIENT buttons
    buttons_frame=gtk_frame_new("Start test (choose an AC): ");
    gtk_box_pack_start(GTK_BOX(main_box),buttons_frame,TRUE,TRUE,2);

    button_box=gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_container_add(GTK_CONTAINER(buttons_frame),button_box);

    // Create a new frame with two buttons for server management
    server_buttons_frame=gtk_frame_new("iPerf server management: ");
    gtk_box_pack_start(GTK_BOX(main_box),server_buttons_frame,TRUE,TRUE,2);

    server_button_box=gtk_button_box_new(GTK_ORIENTATION_VERTICAL);
    gtk_container_add(GTK_CONTAINER(server_buttons_frame),server_button_box);

    // Create three frames + text entries for IP address, IP address to connect to (when calling 'iperf -c'), username and password
    ip_entry=gtk_entry_in_frame_create(GTK_BOX(main_box),"SSH IP address: ",DEFAULT_IP,TRUE);
    connto_ip_entry=gtk_entry_in_frame_create(GTK_BOX(main_box),"(Wireless) IP to connect to: ",DEFAULT_CONNTO_IP,TRUE);
    username_entry=gtk_entry_in_frame_create(GTK_BOX(main_box),"SSH username: ","root",TRUE);
    password_entry=gtk_entry_in_frame_create(GTK_BOX(main_box),"SSH password: ","",FALSE);

    // Create a combobox to select the correct client/server port combination
    clientserver_port_entry=gtk_combobox_in_frame_create(GTK_BOX(main_box),"Server port:",port_labels,G_N_ELEMENTS(port_labels),0);

    // Regulations frame and box to change some iPerf client parameters (using spin buttons)
    regulation_frame=gtk_frame_new("Payload size (B) | Offered traffic (Mbit/s) | Time (s):");
    gtk_box_pack_start(GTK_BOX(main_box),regulation_frame,TRUE,TRUE,2);

    regulation_box=gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_container_add(GTK_CONTAINER(regulation_frame),regulation_box);

    lValue=gtk_adjustment_new(1470.0,16.0,1472.0,10.0,0.0,0.0);
    bValue=gtk_adjustment_new(10.0,1.0,20.0,1.0,0.0,0.0);
    tValue=gtk_adjustment_new(30.0,5.0,MAX_ARRAYSIZE,1.0,0.0,0.0);

    spin_buttons.lValue=GTK_SPIN_BUTTON(gtk_spin_button_new(lValue,30.0,0));
    spin_buttons.bValue=GTK_SPIN_BUTTON(gtk_spin_button_new(bValue,3.0,0));
    spin_buttons.tValue=GTK_SPIN_BUTTON(gtk_spin_button_new(tValue,3.0,0));

    gtk_container_add(GTK_CONTAINER(regulation_box),GTK_WIDGET(spin_buttons.lValue));
    gtk_container_add(GTK_CONTAINER(regulation_box),GTK_WIDGET(spin_buttons.bValue));
    gtk_container_add(GTK_CONTAINER(regulation_box),GTK_WIDGET(spin_buttons.tValue));

    // Create information frame and label, where short text information will be shown
    info_frame=gtk_frame_new("Info: ");
    gtk_box_pack_start(GTK_BOX(main_box),info_frame,TRUE,TRUE,2);

    info_label=gtk_label_new("Program started.");
    gtk_container_add(GTK_CONTAINER(info_frame),info_label);

    // Create color information frame and label, where the information about the current plot will be displayed
    color_info_frame=gtk_frame_new("Current plot (server - incoming traffic): ");
    gtk_box_pack_start(GTK_BOX(main_box),color_info_frame,TRUE,TRUE,2);

    parser_data_struct.label_pointers.color_info_label=gtk_label_new("-");
    gtk_container_add(GTK_CONTAINER(color_info_frame),parser_data_struct.label_pointers.color_info_label);

    // Display the info box, showing the obtained throughput values (placed on the right)
    info_box=gtk_box_new(GTK_ORIENTATION_VERTICAL,2);
    gtk_box_pack_start(GTK_BOX(window_box),info_box,TRUE,TRUE,2);

    // Create goodput information frame and label
    gput_info_frame=gtk_frame_new("Currently measured goodput - incoming traffic (Mbit/s): ");
    gtk_box_pack_start(GTK_BOX(info_box),gput_info_frame,TRUE,TRUE,2);

    parser_data_struct.label_pointers.gput_info_label=gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(parser_data_struct.label_pointers.gput_info_label),"<b><span font=\"70\" foreground=\"#018729\">-</span></b>");
    gtk_container_add(GTK_CONTAINER(gput_info_frame),parser_data_struct.label_pointers.gput_info_label);

    // Create final goodput information frame and label
    final_gput_info_frame=gtk_frame_new("Average goodput value reported by iPerf - incoming traffic (Mbit/s): ");
    gtk_box_pack_start(GTK_BOX(info_box),final_gput_info_frame,TRUE,TRUE,2);

    parser_data_struct.label_pointers.final_gput_info_label=gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(parser_data_struct.label_pointers.final_gput_info_label),"<b><span font=\"70\" foreground=\"blue\">-</span></b>");
    gtk_container_add(GTK_CONTAINER(final_gput_info_frame),parser_data_struct.label_pointers.final_gput_info_label);

    // Create final connection stability information frame and label
    conn_stab_info_frame=gtk_frame_new("Connection stability (max \% variation in goodput values) - incoming traffic: ");
    gtk_box_pack_start(GTK_BOX(info_box),conn_stab_info_frame,TRUE,TRUE,2);

    parser_data_struct.label_pointers.conn_stab_info_label=gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(parser_data_struct.label_pointers.conn_stab_info_label),"<b><span font=\"70\" foreground=\"blue\">-</span></b>");
    gtk_container_add(GTK_CONTAINER(conn_stab_info_frame),parser_data_struct.label_pointers.conn_stab_info_label);

    // Create plot frame
    plot_frame=gtk_frame_new("Plot: ");
    gtk_box_pack_start(GTK_BOX(info_box),plot_frame,TRUE,TRUE,2);

    // Create plot (using the slope library)
    parser_data_struct.plot_pointers.view=slope_view_new();
    figure=slope_figure_new();
 	plot=slope_chart_new();

 	slope_view_set_figure(SLOPE_VIEW(parser_data_struct.plot_pointers.view),figure);

 	scale=slope_xyscale_new_axis("Time (s)","Goodput (Mbit/s)","Measured values");
  	slope_figure_add_scale(SLOPE_FIGURE(figure), scale);

  	// Allocate the arrays which will contain the x-axis and y-axis data to be plotted (NUMSERIES arrays are allocated for both x and y data)
  	parser_data_struct.plot_pointers.currTimePoints=(double **) g_malloc(NUMSERIES*sizeof(double *));
  	parser_data_struct.plot_pointers.currDataPoints=(double **) g_malloc(NUMSERIES*sizeof(double *));
  	for(int i=0;i<NUMSERIES;i++) {
  		parser_data_struct.plot_pointers.currTimePoints[i]=(double *) g_malloc(MAX_ARRAYSIZE*sizeof(double));
  		parser_data_struct.plot_pointers.currDataPoints[i]=(double *) g_malloc(MAX_ARRAYSIZE*sizeof(double));

  		parser_data_struct.plot_pointers.series[i]=slope_xyseries_new();
  		slope_xyseries_set_style(SLOPE_XYSERIES(parser_data_struct.plot_pointers.series[i]),slopeLayoutArray[i]);

  		slope_scale_add_item(scale,parser_data_struct.plot_pointers.series[i]);
  	}

  	// Hide legend
  	legend=slope_scale_get_legend(scale);
  	slope_item_set_is_visible(legend,FALSE);

  	gtk_container_add(GTK_CONTAINER(plot_frame),parser_data_struct.plot_pointers.view);

  	// Initialize the 'static_data' structures
	staticDataStructInit(&static_vars_btn,&static_vars_parse);

	// Set the pointer to the 'static_data' struct inside the parser data struct
	parser_data_struct.static_vars_parse_ptr=&static_vars_parse;

  	// Fill the common GTK data structure
  	common_gtk.info_label=info_label;
	common_gtk.ip_entry=ip_entry;
	common_gtk.connto_ip_entry=connto_ip_entry;
	common_gtk.username_entry=username_entry;
	common_gtk.password_entry=password_entry;
	common_gtk.clientserver_port_entry=clientserver_port_entry;
	common_gtk.parser_pointers=parser_data_struct;
	common_gtk.spin_buttons=spin_buttons;
	common_gtk.static_vars_btn_ptr=&static_vars_btn;
    
    // Create the TOTBTN_CLIENT buttons, using the labels sequentially specified in the "buttonLabels" array
    for(btnidx=0;btnidx<TOTBTN_CLIENT;btnidx++) {
    	buttons[btnidx]=gtk_button_new_with_label(clientButtonLabels[btnidx]);

	    // Fill in the data structure
	    client_data[btnidx].common_gtk_ptr=&common_gtk;
	    client_data[btnidx].btnidx=btnidx;
	    client_data[btnidx].img_area_ptr=img_area;
	    client_data[btnidx].callmode=CLIENT;

	    // Set the callback function (ssh_to_APU()) when the current button is clicked
	    g_signal_connect(buttons[btnidx],"clicked",G_CALLBACK(ssh_to_APU),&(client_data[btnidx]));

	    // Add the current button to the GtkButtonBox widget
	    gtk_container_add(GTK_CONTAINER(button_box),buttons[btnidx]);
	}

	// Create the TOTBTN_SERVER buttons, using the labels sequentially specified in the "serverButtonLabels" array
	for(srvbtnidx=0;srvbtnidx<TOTBTN_SERVER;srvbtnidx++) {
		server_buttons[srvbtnidx]=gtk_button_new_with_label(serverButtonLabels[srvbtnidx]);

		server_data[srvbtnidx].common_gtk_ptr=&common_gtk;
	    server_data[srvbtnidx].btnidx=srvbtnidx;
	    server_data[srvbtnidx].img_area_ptr=server_img_area;
	    server_data[srvbtnidx].callmode=SERVER;

	  	// Set the callback function (ssh_to_APU()) when the current button is clicked
	    g_signal_connect(server_buttons[srvbtnidx],"clicked",G_CALLBACK(ssh_to_APU),&(server_data[srvbtnidx]));

	    // Add the current button to the GtkButtonBox widget
	    gtk_container_add(GTK_CONTAINER(server_button_box),server_buttons[srvbtnidx]);
	}

    // Show the main_window widget and all its children (i.e. all the widgets)
    gtk_widget_show_all(main_window);

    // If openssh is not available, print an error message and exit
    if(system("dpkg -s openssh-client >/dev/null 2>&1")) {
    	GTK_ERROR_MESSAGE_RUN(errormsg,GTK_WINDOW(main_window),"openssh-client is not installed. Please install it.\n");
		gtk_widget_destroy(errormsg);
    	exit(EXIT_FAILURE);
    }

    // Restore locale, to avoid GTK read float numbers with ',' instead of '.'
    setlocale(LC_ALL,"C");

    // "Runs the main loop until gtk_main_quit() is called" (from the official GTK documentation)
    gtk_main();

    // Free previously allocated data
    for(int i=0;i<NUMSERIES;i++) {
    	free(parser_data_struct.plot_pointers.currTimePoints[i]);
    	free(parser_data_struct.plot_pointers.currDataPoints[i]);
    }
    free(parser_data_struct.plot_pointers.currTimePoints);
   	free(parser_data_struct.plot_pointers.currDataPoints);

    // If an error was encountered, display the proper error message
    if(loop_end_errcode==1) {
    	GTK_ERROR_MESSAGE_RUN(errormsg,GTK_WINDOW(main_window),"Error: sshpass is not installed.\nWhen a password is specified, it is required.");
		gtk_widget_destroy(errormsg);
    }

    return loop_end_errcode;
}