clc
clear
close all

R = dicomread ('IM_0001-Bmode');
load R
% the scrol_slice function will show each frame in each slide, and it does have slide
% bar
% scrol_slice(R_Frame);
% N_f is number of frame 
N_f = size (R,4);
% N is block size, and M is search window
% M is from +M to -M, so search window is (2M+1)*(2M+1)
N = 40;
M = 10;


 for frame = 1: N_f-1;

% for frame = 65,68;
    frame_t = frame + 1;
% it's called backward prediction, because it's comparing the fist frame
% with next one, but if we compare a frame with past frame it's called forward prediction,
% and combination of both called biDirectional prediction    
    f_r= R(:,:,:,frame);
    f_t= R(:,:,:,frame_t);
    f_r=rgb2gray (f_r);
    f_t=rgb2gray (f_t);
    [MAD_Min_Blocks,mvx_Blocks,mvy_Blocks] = block_matching_STE(f_r,f_t,N,M);
 end
