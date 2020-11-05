#include <limits>

#include "mainwindow.h"

#define TA_DIGITS_PER_DISPLAY 80
#define TA_SPACING 2
#define OVERSCAN 1.15
#define HO_MARGIN_RESCALE 100
#define TIMEFORMAT "%h:%m:%s"


PerformancePlot::PerformancePlot(Settings* p_glob_settings, SpectrumAdapter* p_spectrumAdapter, QMdiArea* p_mdiArea){
  glob_settings = p_glob_settings;
  spectrumAdapter = p_spectrumAdapter;
  mdiArea = p_mdiArea;

  rnti_x_axis = std::vector<double>(65536);
  int x = 0;
  std::generate(rnti_x_axis.begin(), rnti_x_axis.end(), [&]{ return static_cast<double>(x++); });

  //Generate Window PLOT_A_WINDOW:
  plot_a_window = new QWidget();
  plot_a_window->setObjectName("Plot a");
  plot_a_window->setWindowTitle("Cell Activity");

  gridLayout_a = new QGridLayout(plot_a_window);              //Setup Gridlayout
  gridLayout_a->setSpacing(6);
  gridLayout_a->setContentsMargins(11, 11, 11, 11);
  gridLayout_a->setObjectName(QStringLiteral("gridLayout"));
  gridLayout_a->setSizeConstraint(QLayout::SetMaximumSize);
  gridLayout_a->setContentsMargins(0, 0, 0, 0);

  gridLayout_a->setColumnStretch(0,1);     //Set Stretchfactor 0 = no scaling, 1 = full scaling
  gridLayout_a->setColumnStretch(1,1);
  gridLayout_a->setRowStretch(0,0);
  gridLayout_a->setRowStretch(1,1);
  gridLayout_a->setRowStretch(2,1);

  gridLayout_a->setRowMinimumHeight(0,20);  //Set minimum Size of grid-segment
  gridLayout_a->setRowMinimumHeight(1,100);
  gridLayout_a->setRowMinimumHeight(2,100);
  gridLayout_a->setColumnMinimumWidth(0,200);
  gridLayout_a->setColumnMinimumWidth(1,200);

  plot_mcs_idx = new QCustomPlot(plot_a_window);
  plot_mcs_idx->setObjectName(QStringLiteral("MCS_IDX Plot"));
  plot_mcs_idx->setGeometry(0,0,400,200);

  plot_throughput = new QCustomPlot(plot_a_window);
  plot_throughput->setObjectName(QStringLiteral("MCS_TBS Plot"));
  plot_throughput->setGeometry(0,200,400,200);

  plot_prb     = new QCustomPlot(plot_a_window);
  plot_prb    ->setObjectName(QStringLiteral("MCS_IDX Plot"));
  plot_prb    ->setGeometry(0,400,400,200);

  plot_rnti_hist = new QCustomPlot(plot_a_window);
  plot_rnti_hist->setObjectName(QStringLiteral("RNTI_HIST_PLOT"));
  plot_rnti_hist->setGeometry(0,400,400,200);

  plot_mean_slider_a = new QSlider(plot_a_window);
  plot_mean_slider_a->setGeometry(0, 600, 160, 20);
  plot_mean_slider_a->setMinimum(50);
  plot_mean_slider_a->setMaximum(500);
  plot_mean_slider_a->setValue(250);
  plot_mean_slider_a->setOrientation(Qt::Horizontal);

  plot_mean_slider_label_a = new QLabel(plot_a_window);
  plot_mean_slider_label_a->setGeometry(180, 600, 160, 20);
  plot_mean_slider_label_a->setNum(plot_mean_slider_a->value());

  plot_mean_slider_label_b = new QLabel(plot_a_window);
  plot_mean_slider_label_b->setGeometry(180, 600, 160, 20);
  plot_mean_slider_label_b->setText("           Average (ms)");

  gridLayout_a->addWidget(plot_mcs_idx            , 1, 0);  //Place Widgets into specific grid-segments: row, column
  gridLayout_a->addWidget(plot_throughput            , 1, 1);
  gridLayout_a->addWidget(plot_prb                , 2, 0);
  gridLayout_a->addWidget(plot_rnti_hist          , 2, 1);
  gridLayout_a->addWidget(plot_mean_slider_a        , 0, 0);
  gridLayout_a->addWidget(plot_mean_slider_label_a  , 0, 1);
  gridLayout_a->addWidget(plot_mean_slider_label_b  , 0, 1);

  setupPlot(MCS_IDX_PLOT, plot_mcs_idx);
  setupPlot(MCS_TBS_PLOT, plot_throughput);
  setupPlot(PRB_PLOT    , plot_prb);
  setupPlot(RNTI_HIST   , plot_rnti_hist);

  //Add Subwindow to MDI Area
  plot_a_subwindow = mdiArea->addSubWindow(plot_a_window,Qt::CustomizeWindowHint | Qt::WindowTitleHint);
  //plot_a_subwindow->setWindowTitle("Performance");
  plot_a_window->showMaximized();
}

void PerformancePlot::activate(){

  // Reset metrics
  metrics_dl = CellMetrics();
  metrics_ul = CellMetrics();

  // Initially clear graphs
  plot_mcs_idx->graph(UPLINK)->setData(QVector<double>(), QVector<double>());
  plot_mcs_idx->graph(DOWNLINK)->setData(QVector<double>(), QVector<double>());
  plot_prb->graph(UPLINK)->setData(QVector<double>(), QVector<double>());
  plot_prb->graph(DOWNLINK)->setData(QVector<double>(), QVector<double>());
  plot_throughput->graph(UPLINK)->setData(QVector<double>(), QVector<double>());
  plot_throughput->graph(DOWNLINK)->setData(QVector<double>(), QVector<double>());

  spectrumAdapter->emit_perf_plot_a = true;
  spectrumAdapter->emit_perf_plot_b = true;
  spectrumAdapter->emit_rnti_hist   = true;

  connect (plot_mean_slider_a, SIGNAL(valueChanged(int)),plot_mean_slider_label_a,SLOT(setNum(int)));
  connect (&fps_timer, SIGNAL(timeout()), this, SLOT(replot_perf()));
  fps_timer.start(1000/glob_settings->glob_args.gui_args.perf_fps);

  connect (&avg_timer_uplink, SIGNAL(timeout()), this, SLOT(draw_plot_uplink()));
  avg_timer_uplink.start(plot_mean_slider_a->value());
  connect(plot_mean_slider_a, SIGNAL(valueChanged(int)), &avg_timer_uplink, SLOT(start(int)));

  connect (&avg_timer_downlink, SIGNAL(timeout()), this, SLOT(draw_plot_downlink()));
  avg_timer_downlink.start(plot_mean_slider_a->value());

  connect(&throughput_rescale_timer, SIGNAL(timeout()), this, SLOT(throughput_upper_limit_decay()));
  
  connect(plot_mean_slider_a, SIGNAL(valueChanged(int)), &avg_timer_downlink, SLOT(start(int)));

  connect (spectrumAdapter, SIGNAL(update_perf_plot_b(const ScanLineLegacy*)),SLOT(calc_performance_data(const ScanLineLegacy*)));
  connect (spectrumAdapter, SIGNAL(update_perf_plot_a(const ScanLineLegacy*)),SLOT(calc_performance_data(const ScanLineLegacy*)));
  connect (spectrumAdapter, SIGNAL(update_rnti_hist(const ScanLineLegacy*)),SLOT(draw_rnti_hist(const ScanLineLegacy*)));
}

void PerformancePlot::deactivate(){
  //Deactivate Signals:
  disconnect (spectrumAdapter, SIGNAL(update_perf_plot_a(const ScanLineLegacy*)),this,SLOT(calc_performance_data(const ScanLineLegacy*)));
  disconnect (spectrumAdapter, SIGNAL(update_perf_plot_b(const ScanLineLegacy*)),this,SLOT(calc_performance_data(const ScanLineLegacy*)));
  disconnect (spectrumAdapter, SIGNAL(update_rnti_hist(const ScanLineLegacy*)),this,SLOT(draw_rnti_hist(const ScanLineLegacy*)));

  spectrumAdapter->emit_perf_plot_a = false;
  spectrumAdapter->emit_perf_plot_b = false;
  spectrumAdapter->emit_rnti_hist   = false;

  // Stop timer activities
  fps_timer.stop();
  avg_timer_uplink.stop();
  avg_timer_downlink.stop();
  throughput_rescale_timer.stop();

  fps_timer.disconnect();
  avg_timer_uplink.disconnect();
  avg_timer_downlink.disconnect();
  throughput_rescale_timer.disconnect();

  plot_a_window->disconnect();
  plot_mean_slider_a->disconnect();
}

void PerformancePlot::replot_perf(){
  perf_mutex.lock();
  plot_mcs_idx->replot();
  plot_throughput->replot();
  plot_prb->replot();
  plot_rnti_hist->replot();
  perf_mutex.unlock();
}

void PerformancePlot::calc_performance_data(const ScanLineLegacy *line){
  perf_mutex.lock();
  if (glob_settings->glob_args.spectrum_args.spectrum_line_width != line->total_prb){
    glob_settings->glob_args.spectrum_args.spectrum_line_width = line->total_prb;
    glob_settings->store_settings();
    plot_prb->yAxis->setRange(0, glob_settings->glob_args.spectrum_args.spectrum_line_width);
  }
  // Fix axes
  if(plot_mcs_idx->width()/TA_DIGITS_PER_DISPLAY - TA_SPACING != plot_mcs_idx->xAxis->ticker()->tickCount()){
    plot_mcs_idx->xAxis->ticker()->setTickCount(std::max(plot_mcs_idx->width()/TA_DIGITS_PER_DISPLAY - TA_SPACING, 1));
  }
  if(plot_throughput->width()/TA_DIGITS_PER_DISPLAY - TA_SPACING != plot_throughput->xAxis->ticker()->tickCount()){
    plot_throughput->xAxis->ticker()->setTickCount(std::max(plot_throughput->width()/TA_DIGITS_PER_DISPLAY - TA_SPACING, 1));
  }
  if(plot_prb    ->width()/TA_DIGITS_PER_DISPLAY - TA_SPACING != plot_prb    ->xAxis->ticker()->tickCount()){
    plot_prb->xAxis->ticker()->setTickCount(std::max(plot_prb->width()/TA_DIGITS_PER_DISPLAY - TA_SPACING, 1));
  }

  /*  Assign pointer set according to uplink/downlink case:
     *
     *  There are variables for uplink/downlink calculation each.
     *  Depending on the link type a set of pointers is set to the corresponding set of variables,
     *  the calculation is then operated based on the pointer set.
     */

  CellMetrics* metrics = nullptr;
  if(line->type == SCAN_LINE_PERF_PLOT_A) {
    metrics = &metrics_ul;
    graph_mcs_idx = plot_mcs_idx->graph(UPLINK);
    graph_throughput = plot_throughput->graph(UPLINK);
    graph_prb = plot_prb->graph(UPLINK);
  }
  else if(line->type == SCAN_LINE_PERF_PLOT_B) {
    metrics = &metrics_dl;
    graph_mcs_idx = plot_mcs_idx->graph(DOWNLINK);
    graph_throughput = plot_throughput->graph(DOWNLINK);
    graph_prb = plot_prb->graph(DOWNLINK);
  }

  if(metrics != nullptr) {
    // Sum everything up
    metrics->mcs_tbs += line->mcs_tbs;
    metrics->l_prb   += line->l_prb;
    metrics->mcs_idx += line->mcs_idx;
    metrics->nof_dci++;

    // Check if new TTI has started
    uint32_t tti_now = line->sfn * 10 + line->sf_idx;
    if(metrics->tti_current != tti_now) {   // prefer != instead of < due to wrap-around
      metrics->tti_current = tti_now;
      metrics->nof_tti_with_dci++;
    }
    // On first run, set tti_start
    if(metrics->fresh) {
      metrics->fresh = false;
      metrics->tti_start = tti_now;
    }
  }

  perf_mutex.unlock();
  delete line;
}

void PerformancePlot::draw_plot(LINKTYPE direction, CellMetrics& metrics) {
  double timestamp = QTime::currentTime().msecsSinceStartOfDay()*0.001; // day time in milliseconds
  perf_mutex.lock();

  graph_mcs_idx = plot_mcs_idx->graph(direction);
  graph_throughput = plot_throughput->graph(direction);
  graph_prb = plot_prb->graph(direction);

  if(metrics.nof_tti_with_dci > 0) {    // Only plot if at least one subframe was received
    int delta_ms = 0;
    if (metrics.tti_current > metrics.tti_start) {
      delta_ms = metrics.tti_current - metrics.tti_start;
    }
    else { // integer overflow
      delta_ms = (std::numeric_limits<typeof(metrics.tti_current)>::max() - metrics.tti_start) + metrics.tti_current;
    }
    double throughput_mbps = metrics.mcs_tbs / delta_ms / 1000; // tbs unit: [bit/ms] = [bit/s * 10^-3]; output unit [Mbit/s] = [bit/s * 10^-6];
    if (throughput_mbps > throughput_max*OVERSCAN){
      throughput_max = throughput_mbps;
      plot_throughput->yAxis->setRangeUpper(throughput_max*OVERSCAN);
      #ifdef THRESHOLD_DECAY
      throughput_rescale_timer.start(10000);
      #endif
    }
    graph_mcs_idx->addData(timestamp, metrics.mcs_idx / metrics.nof_dci);
    graph_throughput->addData(timestamp, throughput_mbps);
    graph_prb->addData(timestamp, metrics.l_prb  / delta_ms);
  }
  else {
    graph_mcs_idx->addData(timestamp, 0);
    graph_throughput->addData(timestamp, 0);
    graph_prb->addData(timestamp, 0);
  }

  metrics.mcs_idx = 0;
  metrics.mcs_tbs = 0;
  metrics.l_prb   = 0;
  metrics.nof_dci = 0;
  metrics.nof_tti_with_dci = 0;
  metrics.tti_start = metrics.tti_current;

  // make timestamp axis range scroll with the data (at a constant range size of 10 sec):
  plot_mcs_idx   ->xAxis->setRange(timestamp, 10, Qt::AlignRight);
  plot_prb       ->xAxis->setRange(timestamp, 10, Qt::AlignRight);
  plot_throughput->xAxis->setRange(timestamp, 10, Qt::AlignRight);

  perf_mutex.unlock();
}

void PerformancePlot::draw_plot_downlink(){
  draw_plot(DOWNLINK, metrics_dl);
}

void PerformancePlot::throughput_upper_limit_decay(){
  #ifdef THRESHOLD_DECAY
  perf_mutex.lock();
  // Make upper value smaller to ensure recovery of the plot if only smaller values come in within next time
  // Divide by OVERSCAN^2 to have a decay and compensate the previous overscan
  throughput_upper = throughput_upper/1.5;
  perf_mutex.unlock();
  #endif
}

void PerformancePlot::draw_plot_uplink(){
  draw_plot(UPLINK, metrics_ul);
}

void PerformancePlot::draw_rnti_hist(const ScanLineLegacy *line){
  addData(RNTI_HIST, plot_rnti_hist, line);
  delete line;
}

void PerformancePlot::update_plot_color(){

  plot_mcs_idx->graph(UPLINK)->setPen(QPen(glob_settings->glob_args.gui_args.uplink_plot_color));
  plot_mcs_idx->graph(DOWNLINK)->setPen(QPen(glob_settings->glob_args.gui_args.downlink_plot_color));

  plot_throughput->graph(UPLINK)->setPen(QPen(glob_settings->glob_args.gui_args.uplink_plot_color));
  plot_throughput->graph(DOWNLINK)->setPen(QPen(glob_settings->glob_args.gui_args.downlink_plot_color));

  plot_prb->graph(UPLINK)->setPen(QPen(glob_settings->glob_args.gui_args.uplink_plot_color));
  plot_prb->graph(DOWNLINK)->setPen(QPen(glob_settings->glob_args.gui_args.downlink_plot_color));
}

void PerformancePlot::setupPlot(PlotsType_t plottype, QCustomPlot *plot){

  if(plottype == RNTI_HIST){

    plot->addGraph(); //Blue line
    plot->graph(UPLINK)->setPen(QPen(QColor(1,1,200)));
    plot->graph(UPLINK)->setLineStyle(QCPGraph::lsImpulse);

    QSharedPointer<QCPAxisTickerLog> logTicker(new QCPAxisTickerLog);
    plot->yAxis->setRange(1, 2000);
    plot->yAxis->setScaleType(QCPAxis::stLogarithmic);
    plot->yAxis2->setScaleType(QCPAxis::stLogarithmic);
    plot->yAxis->setTicker(logTicker);
    plot->yAxis2->setTicker(logTicker);

    plot->xAxis->setRange(0,65535);
    plot->xAxis2->setLabel("RNTI Histogram");
    xTicker = QSharedPointer<QCPAxisTickerText>(new QCPAxisTickerText);
    plot->xAxis->setTicker(xTicker);
    plot->xAxis2->setTicker(xTicker);
    xTicker->setTickStepStrategy(QCPAxisTicker::TickStepStrategy::tssReadability);
    xTicker->setTicks(xAT.getTicks(plot_rnti_hist->width()));

    plot->axisRect()->setupFullAxesBox();
  }

  if(plottype == MCS_TBS_PLOT){

    plot->addGraph(); //orange line
    plot->graph(UPLINK)->setName("Uplink");
    plot->graph(UPLINK)->setPen(QPen(glob_settings->glob_args.gui_args.uplink_plot_color));

    plot->addGraph(); //blue line
    plot->graph(DOWNLINK)->setName("Downlink");
    plot->graph(DOWNLINK)->setPen(QPen(glob_settings->glob_args.gui_args.downlink_plot_color));

    // Settings for Axis:
    QSharedPointer<QCPAxisTickerTime> timeTicker(new QCPAxisTickerTime);
    timeTicker->setTimeFormat(TIMEFORMAT);
    timeTicker->setTickStepStrategy(QCPAxisTicker::TickStepStrategy::tssReadability);
    plot->xAxis->setTicker(timeTicker);
    plot->xAxis2->setLabel("Total Throughput [Mbit/s]");
    plot->yAxis->setRange(0,1);
    plot->axisRect()->setupFullAxesBox();
    plot->legend->setVisible(true);
    plot->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignLeft|Qt::AlignTop);
  }

  if(plottype == MCS_IDX_PLOT){

    plot->addGraph(); //orange line
    plot->graph(UPLINK)->setPen(QPen(glob_settings->glob_args.gui_args.uplink_plot_color));
    plot->graph(UPLINK)->setName("Uplink");

    plot->addGraph(); //blue line
    plot->graph(DOWNLINK)->setPen(QPen(glob_settings->glob_args.gui_args.downlink_plot_color));
    plot->graph(DOWNLINK)->setName("Downlink");

    QSharedPointer<QCPAxisTickerTime> timeTicker(new QCPAxisTickerTime);
    timeTicker->setTimeFormat(TIMEFORMAT);
    timeTicker->setTickStepStrategy(QCPAxisTicker::TickStepStrategy::tssReadability);
    plot->xAxis->setTicker(timeTicker);
    plot->xAxis2->setLabel("MCS Index");
    plot->axisRect()->setupFullAxesBox();
    plot->yAxis->setRange(0, 29);
    plot->legend->setVisible(true);
    plot->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignLeft|Qt::AlignTop);
  }

  if(plottype == PRB_PLOT){

    plot->addGraph(); //orange line
    plot->graph(UPLINK)->setPen(QPen(glob_settings->glob_args.gui_args.uplink_plot_color));
    plot->graph(UPLINK)->setName("Uplink");

    plot->addGraph(); //blue line
    plot->graph(DOWNLINK)->setPen(QPen(glob_settings->glob_args.gui_args.downlink_plot_color));
    plot->graph(DOWNLINK)->setName("Downlink");

    QSharedPointer<QCPAxisTickerTime> timeTicker(new QCPAxisTickerTime);
    timeTicker->setTimeFormat(TIMEFORMAT);
    timeTicker->setTickStepStrategy(QCPAxisTicker::TickStepStrategy::tssReadability);
    plot->xAxis->setTicker(timeTicker);
    plot->xAxis2->setLabel("Resourceblocks/Subframe");
    plot->axisRect()->setupFullAxesBox();
    plot->yAxis->setRange(0, glob_settings->glob_args.spectrum_args.spectrum_line_width); //SPECTROGRAM_MAX_LINE_WIDTH); // todo: Get correct prb from
    plot->legend->setVisible(true);
    plot->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignLeft|Qt::AlignTop);
  }
}

void PerformancePlot::addData(PlotsType_t plottype, QCustomPlot *plot, const ScanLineLegacy *data){
  if(plottype == RNTI_HIST){
    static QTime time(QTime::currentTime());
    double key = time.elapsed()/1000.0; // time elapsed since start of demo, in seconds
    static double lastPointKey = 0;
    if(abs(xAT.getPrevW() - plot_rnti_hist->width()) > HO_MARGIN_RESCALE){
      xTicker->setTicks(xAT.getTicks(plot_rnti_hist->width()));
    }

    if ((key - lastPointKey)*1000 > plot_mean_slider_a->value()){ // Update RNTI according to slider
      plot->graph(UPLINK)->data()->clear();
      std::for_each(data->rnti_active_set.begin(), data->rnti_active_set.end(), [plot](rnti_manager_active_set_t i){plot->graph(UPLINK)->data()->add(QCPGraphData(i.rnti, i.frequency));});
      lastPointKey = key;
    }

  }
}

QMdiSubWindow* PerformancePlot::getSubwindow(){
  return plot_a_subwindow;
}

QWidget* PerformancePlot::getWindow(){
  return plot_a_window;
}

void PerformancePlot::setFPS(int fps){
  // fps is in 1/s --> we need msec!
  glob_settings->glob_args.gui_args.perf_fps = fps;
  glob_settings->store_settings();
  if(fps_timer.isActive()){
    fps_timer.start(1000/glob_settings->glob_args.gui_args.perf_fps);
  }
}

PerformancePlot::~PerformancePlot(){
  this->deactivate();
  if(mdiArea != nullptr){ // mdiArea != nullptr --> was activated at least once
    //Close Subwindow:
    mdiArea->setActiveSubWindow(plot_a_subwindow);
    mdiArea->removeSubWindow(plot_a_subwindow);
  }
}
