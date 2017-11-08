% SLICE_SLIDER(3d_volume) plots a 3d volume 1 slice at a time
%   on the current figure.
% A slider bar allows you to slice through the volume.
% Assumes that you want to look at the third dimension using
%   imagesc and the default colormap.
% Frederick Bryan, 2013

function scrol_slice(R_Frame)
% R_Frame = R_Frame(:,:,:,:,1,1,1,1,1);
mid = ceil(size(R_Frame,4)/2);
top = size(R_Frame,4);
step = [1, 1] / (top - 1);
size(R_Frame,4);
h=gcf;

clf(h,'reset');

if length(unique(R_Frame(:)))>500
    R_Frame = img_normalize(R_Frame);
    limits = [0 1];
    ccc = 'gray';
else
    limits = [min(R_Frame(:)) max(R_Frame(:))];
    ccc = 'jet';
end
if limits(2) <= limits(1); limits = [0 1]; end % fix limits (0 case)
figure(h);

uicontrol('Style','slider','Min',1,'Max',top,'Value',mid,...
    'SliderStep', step, 'Position',[10 10 150 25],...
    'Callback',{@changeslice,R_Frame,limits,ccc});
changeslice(get(h,'Children'),[],R_Frame,limits,ccc) % find image in middle
% and pass args to funct
end

function changeslice(src,~,invol,limits,ccc)
% update slice by reploting figure
newsl = get(src,'Value');
%     disp(newsl)
%     imagesc(invol(:,:,round(newsl)),limits);
%         colormap(ccc);
%         title(sprintf('%3.0i',round(newsl)));axis off;
imshow(invol(:,:,:,round(newsl)),limits);
title(sprintf('Slice Number %3.0i',round(newsl)));axis on;
end