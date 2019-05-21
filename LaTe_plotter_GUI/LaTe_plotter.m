function varargout = LaTe_plotter(varargin)
% LATE_PLOTTER MATLAB code for LaTe_plotter.fig
%      LATE_PLOTTER, by itself, creates a new LATE_PLOTTER or raises the existing
%      singleton*.
%
%      H = LATE_PLOTTER returns the handle to a new LATE_PLOTTER or the handle to
%      the existing singleton*.
%
%      LATE_PLOTTER('CALLBACK',hObject,eventData,handles,...) calls the local
%      function named CALLBACK in LATE_PLOTTER.M with the given input arguments.
%
%      LATE_PLOTTER('Property','Value',...) creates a new LATE_PLOTTER or raises the
%      existing singleton*.  Starting from the left, property value pairs are
%      applied to the GUI before LaTe_plotter_OpeningFcn gets called.  An
%      unrecognized property name or invalid value makes property application
%      stop.  All inputs are passed to LaTe_plotter_OpeningFcn via varargin.
%
%      *See GUI Options on GUIDE's Tools menu.  Choose "GUI allows only one
%      instance to run (singleton)".
%
% See also: GUIDE, GUIDATA, GUIHANDLES

% Edit the above text to modify the response to help LaTe_plotter

% Last Modified by GUIDE v2.5 20-May-2019 17:59:35

% Begin initialization code - DO NOT EDIT
gui_Singleton = 1;
gui_State = struct('gui_Name',       mfilename, ...
                   'gui_Singleton',  gui_Singleton, ...
                   'gui_OpeningFcn', @LaTe_plotter_OpeningFcn, ...
                   'gui_OutputFcn',  @LaTe_plotter_OutputFcn, ...
                   'gui_LayoutFcn',  [] , ...
                   'gui_Callback',   []);
if nargin && ischar(varargin{1})
    gui_State.gui_Callback = str2func(varargin{1});
end

if nargout
    [varargout{1:nargout}] = gui_mainfcn(gui_State, varargin{:});
else
    gui_mainfcn(gui_State, varargin{:});
end
% End initialization code - DO NOT EDIT


% --- Executes just before LaTe_plotter is made visible.
function LaTe_plotter_OpeningFcn(hObject, eventdata, handles, varargin)
% This function has no output args, see OutputFcn.
% hObject    handle to figure
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)
% varargin   command line arguments to LaTe_plotter (see VARARGIN)

% Choose default command line output for LaTe_plotter
handles.output = hObject;

% Create the intval array to store the choice about the confidence
% intervals
intval=[0;0;0];

% Set the default radio button choice (i.e. the one for the 95% confidence
% intervals)
set(handles.uiconfint_btn_panel,'selectedobject',handles.radiobutton95);

% Set the filepath initial value (it is used to understand if the user has
% specified a CSV file path or if he/she still has to do so)
handles.filename='0';

% Update handles structure
guidata(hObject, handles);

% UIWAIT makes LaTe_plotter wait for user response (see UIRESUME)
% uiwait(handles.figure1);


% --- Outputs from this function are returned to the command line.
function varargout = LaTe_plotter_OutputFcn(hObject, eventdata, handles) 
% varargout  cell array for returning output args (see VARARGOUT);
% hObject    handle to figure
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)

% Get default command line output from handles structure
varargout{1} = handles.output;


% --- Executes on button press in start.
function start_Callback(hObject, eventdata, handles)
% hObject    handle to start (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)

    % Disable radio buttons when a new session is started
    set(handles.radiobutton90,'Enable','off');
    set(handles.radiobutton95,'Enable','off');
    set(handles.radiobutton99,'Enable','off');
    
    % Check which radio button was selected
    handles.intval(1)=get(handles.radiobutton90,'Value');
    handles.intval(2)=get(handles.radiobutton95,'Value');
    handles.intval(3)=get(handles.radiobutton99,'Value');
    
    % Check if a CSV file path was provided
    if(strcmp(handles.filename,'0')==1)
        errordlg('Please provide a CSV file path','Error');
        return
    end
    
    % Delete any already existing CSV file
    if(isfile(handles.filename)==1)
        set(handles.textinfo,'String','Deleted an already existing CSV file.');
        disp(handles.filename);
        delete(handles.filename);
        pause(1);
    end
    handles.filedata_prev.date='0';
    handles.idx=1;

    % Define timer (look for new data every 100 ms)
    handles.timer = timer('Name','MyTimer',               ...
                          'Period',0.1,                    ... 
                          'StartDelay',0,                 ... 
                          'TasksToExecute',inf,           ... 
                          'ExecutionMode','fixedSpacing', ...
                          'TimerFcn',{@timerCallback,handles.figure1,handles}); 
    
    % Create each series of the plot (n=non raw, u=user-to-user, r=raw,
    % k=KRT)
    hold on;
    handles.nuPlot=errorbar(nan,nan,nan,nan,'bo-');
    handles.ruPlot=errorbar(nan,nan,nan,nan,'gx-');
	handles.nkPlot=errorbar(nan,nan,nan,nan,'r^-');
	handles.rkPlot=errorbar(nan,nan,nan,nan,'kd-');
	xlabel('LaMP payload length (B)');
	ylabel('LaTe average latency (ms)');
	xlim([0 inf]);
	ylim([0 inf]);
	grid on;
	legend('Non raw sockets, User-to-user latency','Raw sockets, User-to-user latency','Non raw sockets, KRT latency','Raw sockets, KRT latency','Location','bestoutside');
	hold off;
    
    guidata(hObject,handles);
    
    % Update the info string
    set(handles.textinfo,'String','Waiting for updates to the CSV file...');
    
    % Start timer
    start(handles.timer);

% --- Executes on button press in stop.
function stop_Callback(hObject, eventdata, handles)
% hObject    handle to stop (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)
    % Check if a CSV file path was provided
    if(strcmp(handles.filename,'0')==1)
        errordlg('Please provide a CSV file path','Error');
        return
    end
    
    % Stop the timer, clear the plot and enable again the radio buttons
    if isfield(handles, 'timer')
        stop(handles.timer);
        delete(handles.nuPlot);
        delete(handles.ruPlot);
        delete(handles.nkPlot);
        delete(handles.rkPlot);
        hLegend=findobj('type','legend');
        delete(hLegend);
        set(handles.textinfo,'String','Program stopped.');
        
        % Enable radio buttons
        set(handles.radiobutton90,'Enable','on');
        set(handles.radiobutton95,'Enable','on');
        set(handles.radiobutton99,'Enable','on');
    end
    
function timerCallback(~,~,hFigure,fullhandles)
    % Get the handles
    handles=guidata(hFigure);
    if(~isempty(handles))
        if ~isempty(handles)
            if(isfile(fullhandles.filename)==1)
                % Get the data related to the current file (including its
                % last modified date) and compare it to the stored one, to
                % identify any modification to the CSV file
                filedata=dir(fullhandles.filename);
                if(strcmp(filedata.date,handles.filedata_prev.date)==0)
                    set(handles.textinfo,'String',['New data is available (' num2str(handles.idx) ')']);
                    % The file has been modified, i.e. a new line has been added
                    csvData=csvreadadvanced(handles.filename,[1 2 3 4 5 10],[1 2 3 4 5 10]);
                    
                    % Update the correct plot
                    if(strcmp(csvData.SocketType{handles.idx},'Non raw')==1 && strcmp(csvData.LatencyType{handles.idx},'User-to-user')==1)
                        [xArray,yArray,yArray_deltaplus,yArray_deltaminus]=updateArrays(get(handles.nuPlot,'XData'),get(handles.nuPlot,'YData'),get(handles.nuPlot,'YPositiveDelta'),get(handles.nuPlot,'YNegativeDelta'),csvData,handles.idx,handles.intval);
                        set(handles.nuPlot,'XData',xArray,'YData',yArray,'YPositiveDelta',yArray_deltaplus,'YNegativeDelta',yArray_deltaminus);
                    elseif(strcmp(csvData.SocketType{handles.idx},'Raw')==1 && strcmp(csvData.LatencyType{handles.idx},'User-to-user')==1)
                        [xArray,yArray,yArray_deltaplus,yArray_deltaminus]=updateArrays(get(handles.ruPlot,'XData'),get(handles.ruPlot,'YData'),get(handles.ruPlot,'YPositiveDelta'),get(handles.ruPlot,'YNegativeDelta'),csvData,handles.idx,handles.intval);
                        set(handles.ruPlot,'XData',xArray,'YData',yArray,'YPositiveDelta',yArray_deltaplus,'YNegativeDelta',yArray_deltaminus);
                    elseif(strcmp(csvData.SocketType{handles.idx},'Non raw')==1 && strcmp(csvData.LatencyType{handles.idx},'KRT')==1)
                        [xArray,yArray,yArray_deltaplus,yArray_deltaminus]=updateArrays(get(handles.nkPlot,'XData'),get(handles.nkPlot,'YData'),get(handles.nkPlot,'YPositiveDelta'),get(handles.nkPlot,'YNegativeDelta'),csvData,handles.idx,handles.intval);
                        set(handles.nkPlot,'XData',xArray,'YData',yArray,'YPositiveDelta',yArray_deltaplus,'YNegativeDelta',yArray_deltaminus);
                    else
                        [xArray,yArray,yArray_deltaplus,yArray_deltaminus]=updateArrays(get(handles.rkPlot,'XData'),get(handles.rkPlot,'YData'),get(handles.rkPlot,'YPositiveDelta'),get(handles.rkPlot,'YNegativeDelta'),csvData,handles.idx,handles.intval);
                        set(handles.rkPlot,'XData',xArray,'YData',yArray,'YPositiveDelta',yArray_deltaplus,'YNegativeDelta',yArray_deltaminus);
                    end
                    % Move to the next line of the CSV file (the index
                    % 'idx' is used by updateArrays() to parse the correct
                    % CSV file line)
                    handles.idx=handles.idx+1;
                end
                handles.filedata_prev=filedata;
            end
            guidata(hFigure,handles);
        end
    end


% --- Executes on button press in filelocation_btn.
function filelocation_btn_Callback(hObject, eventdata, handles)
% hObject    handle to filelocation_btn (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)
    % Get the CSV file full path from the user
    filepath=inputdlg('Insert the CSV file path','CSV file path');
    handles.filename=filepath{1};
    
    set(handles.textCSVpath,'String',filepath);
    
    guidata(hObject,handles);
