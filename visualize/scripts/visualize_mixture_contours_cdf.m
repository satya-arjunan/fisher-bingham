function [] = visualize_mixture_contours_cdf(K,pdf)

  addpath('export_fig');

  isvmf = strcmp(pdf,'vmf');
  bins_folder = '';
  outfile = '';
  if (isvmf == 1)
    bins_folder = '../sampled_data/bins_vmf/';
    outfile = 'vmf_mix';
  elseif (isvmf == 0)
    bins_folder = '../sampled_data/bins_kent/';
    outfile = 'kent_mix';
  end
  disp(strcat('bins_folder: ',{' '},bins_folder)); 
 
  % figure properties
  fig = figure();
  caxis([0 1]); % colormap range
  %axis equal
  hold on;
  set(gcf, 'Color', 'w');
  xlabel('Longitude','fontsize',20);
  ylabel('Co-latitude','fontsize',20);
  set(gca,'Ylim',[0 180]);
  set(gca,'Xlim',[0 360]);
  set(gca,'xtick',[0:60:360],'fontsize',12);
  set(gca,'ytick',[0:30:180],'fontsize',12);
  view ([0 90]);

  % plot the contours 
  phi = 0:1:359.9;  % meshgrid columns
  theta = 0:1:179.9;  % meshgrid rows 
%  min_val = 0;
%  max_val = 0;
  for k = 1:K
    %k
    data_file = strcat(bins_folder,'comp',num2str(k),'_prob_bins2D.dat');

    prob_bins = load(data_file);
    [max_val_bins max_index] = max(prob_bins(:));
    prob_bins = prob_bins / sum(sum(prob_bins));
    [sorted_prob_bins, indexes] = sort(prob_bins(:),1,'descend');
    sorted_prob_bins_cumsum = cumsum(sorted_prob_bins);
    cdf_bins_list = ones(length(sorted_prob_bins),1);
    cdf_bins_list(indexes) = sorted_prob_bins_cumsum;
    cdf_bins = reshape(cdf_bins_list,size(prob_bins));

    min_val = min(cdf_bins(:));
    max_val = max(cdf_bins(:));
    range = max_val - min_val;
    cdf_bins = (cdf_bins - min_val) / range; % in [0,1]

    level = 0.8;
    norm_level = (level - min_val) / range;
    contour_levels = [norm_level norm_level];
    [C,h] = contour(cdf_bins,contour_levels,'LineWidth',2,'LineColor','black');
    %[C,h] = contour(cdf_bins,1,'LineWidth',2,'LineColor','black');
    %clabel(C,h);

%    [c1,h1] = contour(cdf_bins,contour_levels,'LineWidth',2,'LineColor','black');
%    % Take all the info from the contourline output argument:
%    i0 = 1;
%    i2 = 1;       
%    while i0 <  length(c1)
%        i1 = i0+[1:c1(2,i0)];
%        zLevel(i2) = c1(1,i0);
%        hold on
%        % And plot it with dashed lines:
%        %ph(i2) = plot(c1(1,i1),c1(2,i1),'k--','linewidth',2); 
%        ph(i2) = dashline(c1(1,i1),c1(2,i1),1.5,3,1.5,3,'Color','black','linewidth',2); 
%        i0 = i1(end)+1;
%        i2 = i2+1;
%    end
%    % Scrap the contourlines:
%    delete(h1);

    [row col] = ind2sub(size(prob_bins),max_index);
    cx = phi(col);
    cy = theta(row);
    ht = text(cx,cy,num2str(k),'Color','red');

%    hcl = clabel(C,'Color','red');
%    for i=2:2:length(hcl)
%      old_label = get(hcl(i),'String');
%      new_label = num2str(k);
%      set(hcl(i),'String',new_label);
%    end
  end  

  %uistack(hs,'bottom');
  %uistack(ht,'top');

  % plot the entire mixture density
  data_file = strcat(bins_folder,'mixture_density.dat');
  M = load(data_file);

  density = M(:,4);
  min1 = min(density);
  max1 = max(density);
  range1 = max1 - min1;
  %range = max_val - min_val;
  %norm_density = min_val + (((density - min1) / range1) * range);
  norm_density = (density - min1) / range1; % in [0,1]
  n = size(M,1);
  angles = zeros(n,2);
  for i=1:n
    x = M(i,1);
    y = M(i,2);
    z = M(i,3);
    [phi, theta] = cartesian2spherical(x,y,z);
    angles(i,1) = phi;
    angles(i,2) = theta;
  end
%  min(norm_density)
%  max(norm_density)
  hs = scatter3(angles(:,1),angles(:,2),norm_density,2,'cdata',norm_density);

  %colorbar
  output_fig = strcat('../figs/',outfile,'.fig');
  output_eps = strcat('../figs/',outfile,'.eps');
  output_pdf = strcat('../figs/',outfile,'.pdf');

  %saveas(gcf,output_fig);
  %print2eps(output_eps);
  %eps2pdf(output_eps,output_pdf,1);
  %export_fig(output_pdf,'-pdf');

end
