function data = csvreadadvanced( filename,string_indeces,exclude_indeces )
% csvreadadvanced: function to read .csv files produced by any external
% app. A structure with properly named fields is returned.
%   Use: [time,data]=csvreadadvanced(filename,string_indeces,exclude_indeces)
%   Input args: filename -> .csv file name
%               string_indeces -> indeces of columns, inside the .csv file,
%               that can also contain non numeric data
%               exclude_indeces -> indeces of columns that can contain data
%               that should be kept as string (in this case cell arrays are
%               returned). Otherwise, the data of string cells is converted
%               into null (0) numeric data.
%   Output: data -> struct containing all the csv column. The name of the struct
%   fields should reflect the name of the csv columns.
%   
%   Authors: Francesco Raviglione, Lorenzo Racca (version 2.1-apu)
    
    % Open file and read first line (column names)
    fid=fopen(filename,'rt');
    CSVfield=fgetl(fid);
        
    % Prepare columns name to be properly handled
    CSVfield=strrep(CSVfield,', ',',');
    CSVfield=strrep(CSVfield,' ','_');
    CSVfield=strrep(CSVfield,'(','_');
    CSVfield=strrep(CSVfield,')','_');
    CSVfield=strrep(CSVfield,'/','');
    CSVfield=strrep(CSVfield,'__','_');
    CSVfield=strrep(CSVfield,'%','perc');
    CSVfield=strrep(CSVfield,'°','deg');
    CSVfield=strrep(CSVfield,'-','_');
    CSVfield=strrep(CSVfield,'+','plus');
    
    % Get the column names (launched here to allow the determination of
    % length(names))
    names=strsplit(CSVfield,',');
    names=strip(names,'right','_');
    
    % Determine where to read strings or numeric data, depending on the
    % indeces specified.
    scan_type=[];
    for i=1:length(names)
        if(ismember(i,string_indeces))
            scan_type=[scan_type '%s '];
        else
            scan_type=[scan_type '%f '];
        end
    end
    % Remove right-most space
    scan_type=strip(scan_type,'right',' ');
    
    % Read raw data from CSV file
    CSVdata=textscan(fid,scan_type,'Delimiter',',');
    fclose(fid);
    
    % Generate data structure
    data=struct();
    for k=1:length(names)
       if(ismember(k,string_indeces(~ismember(string_indeces,exclude_indeces))))
            data.(names{k})=0;
       else
            data.(names{k})=CSVdata{k};
       end
    end

    % Translate cell arrays into double arrays
    for index=string_indeces(~ismember(string_indeces,exclude_indeces))
        for i=1:length(CSVdata{index})
            if(isnan(str2double(CSVdata{index}(i))))
                data.(names{index})(i)=0;
            else
                data.(names{index})(i)=str2double(CSVdata{index}(i));
            end
        end
        % Transpose horizontal arrays
        data.(names{index})=data.(names{index})';
    end
end