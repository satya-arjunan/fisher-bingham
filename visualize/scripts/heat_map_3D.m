function [] = heat_map_3D(file_name)

  addpath('export_fig');

  % draw a unit sphere
  %n = 25;
  %r = ones(n, n); % radius is 1 
  %[th, phi] = meshgrid(linspace(0, pi, n), linspace(0, 2*pi, n));
  %[x,y,z]= sph2cart(th, phi, r);
  %surface(x,y,z,'FaceColor','none');
  %hold on;

  % plot data
  M = load(file_name);
  x = M(:,1);
  y = M(:,2);
  z = M(:,3);
  density = M(:,4);

  fig = figure();

  h=scatter3(x,y,z,'cdata',density);

  view ([0 90]);
  axis equal;
  set(gcf, 'Color', 'w');
  set(gcf,'defaultaxesfontname','Arial');
  
  set(gca,'Xlim',[-1 1]);
  set(gca,'Ylim',[-1 1]);
  set(gca,'Zlim',[-1 1]);

  %view ([180 90]);

  xlabel('X_2','fontsize',20);
  ylabel('X_3','fontsize',20);
  %zlabel('Z');

  ylabh = get(gca,'YLabel');
  set(ylabh,'Position',[-1.2 0 0]);

  set(gca,'xtick',[-1,-0.5,0,0.5,1],'fontsize',18);
  set(gca,'ytick',[-1,-0.5,0,0.5,1],'fontsize',18);
  %set(gca,'ztick',[]);

  file_name = '../figs/k10_e5_heatmap';
  output_fig = strcat(file_name,'.fig');
  output_eps = strcat(file_name,'.eps');
  output_pdf = strcat(file_name,'.pdf');
  output_jpg = strcat(file_name,'.jpg');

  %print(file_name,'-dpdf');
  %saveas(gcf,output_fig);

  %print2eps(output_eps);
  %eps2pdf(output_eps,output_pdf,1);

  export_fig(output_pdf,'-pdf','-r100');
  %export_fig(output_jpg,'-jpg');

end
