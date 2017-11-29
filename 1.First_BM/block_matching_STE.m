
% inputs are:
% f_r --- reference image
% f_t --- target image
% N --- block size
% M --- search window

% output are:
% MAD_Min_Blocks
% mvx_Blocks
% mvy_Blocks

function [MAD_Min_Blocks,mvx_Blocks,mvy_Blocks] = block_matching_STE(f_r,f_t,N,M)

Height = size(f_r,1);
Width = size(f_r,2);
figure,imshow(f_r);
figure,imshow(f_t);


%  this command will determine the number of block in X and Y axis for each
%  search window 
num_blk_x = ceil((Height/N));
num_blk_y = ceil((Width/N));
%  it's creating an empty blocks for MAD_min, mvx, and mvy with zeros
%  values to fill it with outputs later
MAD_Min_Blocks = zeros(num_blk_x,num_blk_y);
mvx_Blocks = zeros(num_blk_x,num_blk_y);
mvy_Blocks = zeros(num_blk_x,num_blk_y);


for i = 1:N:Height-N;
     for    j = 1:N:Width-N;
     
        % MAD_Matrix, mvx, and mvy are a quadrangle with 21 * 21 height and
        % width which are include only zeros values for a search window
        % movement,and +1 is included, because it should count the center
        % of quadrangle
        MAD_Matrix = zeros(2*M+1);
        mvx = zeros(2*M+1);
        mvy = zeros(2*M+1);
        %height and width of current search location show with K and L
        for k = -M:1:M
            for l = -M:1:M
                %MAD=sum(sum(abs(f_r(i:i+N,j:j+N+1)-f_t(i+k:i+k+N-1,j+l:j+l+N-1))));
                MAD = 0;
                %u and v are displacement for the height and width
                for u = 0:N-1
                    for v = 0:N-1
                        if ((u+k > 0)&&(u+k < Height + 1)&&(v+l > 0)&&(v+l < Width + 1))
                            %a is reference image with consider u and v displacement
                            % b is target image with consider u, v, in new search location
                            % (k,l)
                            % MAD is mean of differences between refernce and target image
                            a = double(f_r(i+u,j+v));
                            b = double(f_t(i+u+k,j+v+l));
                            MAD = MAD + abs(a-b);
                        end
                    end
                end
                
                MAD = MAD/(N*N);
                %                     if (MAD<MAD_min)
                %                         MAD_min = MAD;
                %                         dx = k;
                %                         dy = l;
                %                         if (MAD_min<1)
                %                             dy = 0;
                %                             dx = 0;
                %                         end
                %                     end
                
                % These matrix are for current seach location, M+1 is because it should count the center of quadrangle
                MAD_Matrix(k+M+1,l+M+1) = MAD;
                mvx(k+M+1,l+M+1) = k;
                mvy(k+M+1,l+M+1) = l;
            end
        end
        %  MAD_min wil find and keep the minest MAD
        MAD_min = min(MAD_Matrix(:));
        [dx, dy] = find(MAD_Matrix==MAD_min);
        dx = dx-M-1;
        dy = dy-M-1;
        dx = round(mean(dx));
        dy = round(mean(dy));
        iblk = ceil((i+1)/(N+1));
        jblk = ceil((j+1)/(N+1));
     
        %these are our outputs
        mvx_Blocks(iblk,jblk) = double(dx);
        mvy_Blocks(iblk,jblk) = double(dy);
        MAD_Min_Blocks(iblk,jblk) = double(MAD_min);
       
        %             figure, imshow(f_t);
        %             hold on;
        arrow([j i],[j+dy i+dx], 3,'Color','y');
     
    end
end
end


     
