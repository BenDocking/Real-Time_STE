% loads the ultrasound data saved by the Sonix software
% Inputs:  filename - The path of the data to open
% Return:
%     Im -         The image data returned into a 3D array (h, w, numframes)
%     header -     The file header information

function [Im,header] = SureScan_Read_UltraSonix_Data(filename)

if nargin < 1
    [filename, path] = uigetfile('*.*');
    filename = [path filename];
end

fileID = fopen(filename, 'r');

if( fileID == -1)
    error('Cannot open file');
end

%------------------------------------------------------------------------------------------------------------------
% read the header info
hinfo = fread(fileID, 19, 'int32');

% load the header information into a structure
% Ulterius data file header. All integers are 4 bytes.
% 1: data type - data types can also be determined by file extensions
% 2: number of frames in file
% 3: width - number of vectors for raw data, image width for processed data
% 4: height - number of samples for raw data, image height for processed data
% 5: data sample size in bits
% 6: roi - upper left (x)
% 7: roi - upper left (y)
% 8: roi - upper right (x)
% 9: roi - upper right (y)
% 10: roi - bottom right (x)
% 11: roi - bottom right (y)
% 12: roi - bottom left (x)
% 13: roi - bottom left (y)
% 14: probe identifier - additional probe information can be found using this id
% 15: transmit frequency
% 16: sampling frequency
% 17: data rate - frame rate or pulse repetition period in Doppler modes
% 18: line density - can be used to calculate element spacing if pitch and native # elements is known
% 19: extra information - ensemble for color RF

header = struct('filetype', 0, 'nframes', 0, 'w', 0, 'h', 0, 'ss', 0, 'ul', [0,0], 'ur', [0,0], 'br', [0,0], 'bl', [0,0], 'probe',0, 'txf', 0, 'sf', 0, 'dr', 0, 'ld', 0, 'extra', 0);
header.filetype = hinfo(1);
header.nframes = hinfo(2);
header.w = hinfo(3);
header.h = hinfo(4);
header.ss = hinfo(5);
header.ul = [hinfo(6), hinfo(7)];
header.ur = [hinfo(8), hinfo(9)];
header.br = [hinfo(10), hinfo(11)];
header.bl = [hinfo(12), hinfo(13)];
header.probe = hinfo(14);
header.txf = hinfo(15);
header.sf = hinfo(16);
header.dr = hinfo(17);
header.ld = hinfo(18);
header.extra = hinfo(19);

header.filename = filename
%------------------------------------------------------------------------------------------------------------------

% Ulterius data types, using Hexadecimal Numbers (base 16)
% 
% udtScreen                   = 0x00000001,
% udtBPre                     = 0x00000002,
% udtBPost                    = 0x00000004,
% udtBPost32                  = 0x00000008,
% udtRF                       = 0x00000010,
% udtMPre                     = 0x00000020,
% udtMPost                    = 0x00000040,
% udtPWRF                     = 0x00000080,
% udtPWSpectrum               = 0x00000100,
% udtColorRF                  = 0x00000200,
% udtColorCombined            = 0x00000400,
% udtColorVelocityVariance    = 0x00000800,
% udtElastoCombined           = 0x00002000,
% udtElastoOverlay            = 0x00004000,
% udtElastoPre                = 0x00008000,
% udtECG                      = 0x00010000

v=[];
Im2=[];


switch (header.filetype)
    
    case 2 %.bpr
        for frame_count = 1:header.nframes
            [v,count] = fread(fileID,header.w*header.h,'uchar=>uchar');
            Im(:,:,frame_count) = reshape(v,header.h,header.w);
        end
        
    case 4 %postscan B .b8
        Im = zeros(header.h,header.w,header.nframes, 'uint8');
        for frame_count = 1:header.nframes
            Im(:,:,frame_count) = (fread(fileID, [header.w, header.h], 'uint8'))'; %fliplr((fread(fileID, [header.w, header.h], 'uint8'))');
            imshow(Im(:,:,frame_count)); pause(0.01);
        end
        
    case 8 %postscan B .b32
        Im = zeros(header.h,header.w,3,header.nframes, 'uint8');
        for frame_count = 1:header.nframes
            temp1 = fread(fileID,header.w*header.h*4,'uint8');
            temp1 = reshape(temp1,4,header.w,header.h);       
            for i=1:3
                temp2(:,:) = temp1(i,:,:);
                Im(:,:,i,frame_count) = fliplr(temp2');
            end
            imshow(Im(:,:,:,frame_count)); pause(0.001);
        end
        
    case 16 % .rf
        for frame_count = 1:header.nframes
            [v,count] = fread(fileID,header.w*header.h,'int16'); 
            Im(:,:,frame_count) = int16(reshape(v,header.h,header.w));
        end
        
    case 32 % .mpr
        Im = zeros(header.h,header.nframes, 'uint8');
        for frame_count = 1:header.nframes
            temp = fread(fileID,header.h,'uint8'); 
            Im(:,frame_count) = temp;
        end
        %imshow(Im);
        
    case 64 % .m
        temp = fread(fileID,'uint8');
        Im = uint8((reshape(temp,header.w,header.h))');
       
    case 128 %.drf
        Im = zeros(header.h,header.nframes, 'int16');
        Im = zeros(header.h,header.nframes);
        for frame_count = 1:header.nframes
            [v,count] = fread(fileID,header.h,'int16'); 
            Im(:,frame_count) = v;
        end
             
    case 512 %crf
        Im = zeros(header.h,header.w*header.extra,header.nframes, 'int16');
        for frame_count = 1:header.nframes
            [v,count] = fread(fileID,header.extra*header.w*header.h,'int16'); 
            Im(:,:,frame_count) = reshape(v,header.h,header.w*header.extra);
            %to obtain data per packet size use
            % Im(:,:,:,frame_count) = reshape(v,header.h,header.w,header.extra);
        end
           
    case 256 %.pw
        Im = zeros(header.h,header.w,header.nframes, 'uint8');
        for frame_count = 1:header.nframes
            [v,count] = fread(fileID,'uint8');
            temp = reshape(v,header.w,header.h);  
            Im(:,:,frame_count) = imrotate(temp,-90);
        end
        
    case 1024 %.col
        
        Im = zeros(header.h,header.w,3,header.nframes, 'uint8');
        for frame_count = 1:header.nframes
            temp1 = fread(fileID,header.w*header.h*4,'uint8');
            temp1 = reshape(temp1,4,header.w,header.h);       
            for i=1:3
                temp2(:,:) = temp1(i,:,:);
                Im(:,:,i,frame_count) = fliplr(temp2');
            end
            %imshow(Im(:,:,:,frame_count)); pause(0.001);
        end      
%         for frame_count = 1:header.nframes
%             [v,count] = fread(fileID,header.w*header.h,'int'); 
%             temp = reshape(v,header.w,header.h);
%             temp2 = imrotate(temp, -90); 
%             Im(:,:,frame_count) = SureScan_mirror(temp2,header.w);
%         end
        
    case 2048 %color .cvv
        
        Im = zeros(header.h,header.w,header.nframes, 'uint8'); Im2 = Im;
        for frame_count = 1:header.nframes
            % velocity data
            [v,count] = fread(fileID,header.w*header.h,'uint8'); 
            temp = reshape(v,header.w,header.h); 
            temp2 = imrotate(temp, -90);
            Im(:,:,frame_count) = SureScan_mirror(temp2,header.w);
        
            % sigma
            [v,count] =fread(fileID, header.w*header.h,'uint8');
            temp = reshape(v,header.w, header.h);
            temp2 = imrotate(temp, -90);
            Im2(:,:,frame_count) = SureScan_mirror(temp2,header.w);
        end
        header.Im2 = Im2;
        
    case 4096 %color vel 
        for frame_count = 1:header.nframes
            [v,count] = fread(fileID,header.w*header.h,'uchar=>uchar'); 
            temp = reshape(v,header.w,header.h); 
            temp2 = imrotate(temp, -90);
            Im(:,:,frame_count) = SureScan_mirror(temp2,header.w);
        end
        
    case 8192 %.el  
        for frame_count = 1:header.nframes
            [v,count] = fread(fileID,header.w*header.h,'int32'); 
            temp = reshape(v,header.w,header.h);
            temp2 = imrotate(temp, -90);
            Im(:,:,frame_count) = mirror(temp2,header.w);
        end
        
    case 16384 %.elo 
        for frame_count = 1:header.nframes
            [v,count] = fread(fileID,header.w*header.h,'uchar=>uchar'); 
            temp = int16(reshape(v,header.w,header.h));
            temp2 = imrotate(temp, -90); 
            Im(:,:,frame_count) = mirror(temp2,header.w);
        end
        
    case 32768 %.epr  
        for frame_count = 1:header.nframes
            [v,count] = fread(fileID,header.w*header.h,'uchar=>uchar'); 
            Im(:,:,frame_count) = int16(reshape(v,header.h,header.w));
        end
        
    case 65536 %.ecg
        Im = zeros(header.h*header.w,header.nframes, 'uint8');
        for frame_count = 1:header.nframes
            [v,count] = fread(fileID,header.w*header.h,'uint8');
            Im(:,frame_count) = v;
        end
        
    case 131072 %.gps 
        for frame_count = 1:header.nframes
            gps_posx(:, frame_count) =  fread(fileID, 1, 'double');   %8 bytes for double
            gps_posy(:, frame_count) = fread(fileID, 1, 'double');
            gps_posz(:, frame_count) = fread(fileID, 1, 'double');
            gps_s(:,:, frame_count) = fread(fileID, 9, 'double');    %position matrix
            gps_time(:, frame_count) = fread(fileID, 1, 'double');
            gps_quality(:, frame_count) = fread(fileID, 1, 'ushort'); % 2 bytes for unsigned short
            Zeros(:, frame_count) =  fread(fileID, 3, 'uint16');     % 6 bytes of padded zeroes
        end
       
    otherwise
        disp('Data not supported');
        
end
    
fclose(fileID);
% close all;

