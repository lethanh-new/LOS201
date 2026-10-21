set terminal pngcairo size 1600,1000 enhanced font "DejaVu Sans,12"
set output "logs/latency_histogram_comparison.png"

set multiplot layout 2,1 title \
"LAB-03: Cyclictest Latency - Ubuntu Standard Kernel" \
font "DejaVu Sans,17"

set grid
set key top right
set xlabel "Scheduling latency (microseconds)"
set ylabel "Number of samples"

# Sum four measurement threads.
# Column 1 = latency bin.
# Columns 2-5 = counts for threads 0-3.

set title "Latency Distribution: 0-80 us"
set xrange [0:80]
set yrange [0:*]
unset logscale y

plot \
  "logs/hist_standard.txt" using 1:($2+$3+$4+$5) \
    with lines linewidth 2 title "Baseline", \
  "logs/hist_stress.txt" using 1:($2+$3+$4+$5) \
    with lines linewidth 2 title "Under stress"

set title "Recorded Histogram: 0-199 us (2 stress overflows above range)"
set xrange [0:199]
set yrange [0.9:*]
set logscale y

plot \
  "logs/hist_standard.txt" using 1:($2+$3+$4+$5) \
    with points pointtype 7 pointsize 0.65 title "Baseline", \
  "logs/hist_stress.txt" using 1:($2+$3+$4+$5) \
    with points pointtype 5 pointsize 0.7 title "Under stress"

unset multiplot
