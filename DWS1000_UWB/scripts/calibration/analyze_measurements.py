#!/usr/bin/env python3
"""
DW1000 Measurement Analysis Tool

Purpose: Analyze ranging measurements for calibration
Features:
- Calculate statistics (mean, std, min, max)
- Generate plots (if matplotlib available)
- Export results to JSON/CSV
- Validation report generation
"""

import sys
import argparse
import json
import csv
from pathlib import Path
from typing import List, Dict, Optional
import statistics


def parse_csv_data(file_path: Path) -> List[float]:
    """
    Parse CSV data from calibration firmware.
    Expected format: timestamp,distance_m
    """
    distances = []

    try:
        with open(file_path, 'r') as f:
            for line in f:
                line = line.strip()

                # Skip empty lines and comments
                if not line or line.startswith('#'):
                    continue

                # Try to parse as CSV
                parts = line.split(',')

                if len(parts) >= 2:
                    try:
                        # Second column should be distance in meters
                        distance = float(parts[1])

                        # Filter out obviously bad values
                        if 0.01 < distance < 100.0:  # Between 1cm and 100m
                            distances.append(distance)
                    except ValueError:
                        # Not a valid number, skip
                        continue
                elif len(parts) == 1:
                    # Maybe just distance values
                    try:
                        distance = float(parts[0])
                        if 0.01 < distance < 100.0:
                            distances.append(distance)
                    except ValueError:
                        continue

    except FileNotFoundError:
        print(f"ERROR: File not found: {file_path}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"ERROR: Failed to parse {file_path}: {e}", file=sys.stderr)
        sys.exit(1)

    return distances


def calculate_statistics(distances: List[float], actual_distance: float) -> Dict:
    """
    Calculate statistics for distance measurements.
    """
    if not distances:
        return {
            'num_samples': 0,
            'mean_distance_m': 0.0,
            'median_distance_m': 0.0,
            'stddev_m': 0.0,
            'stddev_cm': 0.0,
            'min_distance_m': 0.0,
            'max_distance_m': 0.0,
            'range_m': 0.0,
            'actual_distance_m': actual_distance,
            'error_m': 0.0,
            'error_cm': 0.0,
            'error_percent': 0.0,
        }

    mean_dist = statistics.mean(distances)
    median_dist = statistics.median(distances)
    stddev = statistics.stdev(distances) if len(distances) > 1 else 0.0
    min_dist = min(distances)
    max_dist = max(distances)
    range_dist = max_dist - min_dist

    error_m = mean_dist - actual_distance
    error_cm = error_m * 100.0
    error_percent = (error_m / actual_distance) * 100.0 if actual_distance > 0 else 0.0

    return {
        'num_samples': len(distances),
        'mean_distance_m': mean_dist,
        'median_distance_m': median_dist,
        'stddev_m': stddev,
        'stddev_cm': stddev * 100.0,
        'min_distance_m': min_dist,
        'max_distance_m': max_dist,
        'range_m': range_dist,
        'actual_distance_m': actual_distance,
        'error_m': error_m,
        'error_cm': error_cm,
        'error_percent': error_percent,
    }


def calculate_antenna_delay_adjustment(error_m: float) -> int:
    """
    Calculate antenna delay adjustment based on measurement error.

    Formula:
        error_per_device = error_m / 2.0
        adjustment = error_per_device * 213.14
    """
    error_per_device = error_m / 2.0
    adjustment = int(error_per_device * 213.14)
    return adjustment


def print_statistics(stats: Dict, quiet: bool = False):
    """
    Print statistics to console.
    """
    if quiet:
        return

    print("\n" + "=" * 60)
    print("Measurement Statistics")
    print("=" * 60)
    print(f"Samples:          {stats['num_samples']}")
    print(f"Actual Distance:  {stats['actual_distance_m']:.3f} m")
    print(f"Measured Mean:    {stats['mean_distance_m']:.3f} m")
    print(f"Measured Median:  {stats['median_distance_m']:.3f} m")
    print(f"Min:              {stats['min_distance_m']:.3f} m")
    print(f"Max:              {stats['max_distance_m']:.3f} m")
    print(f"Range:            {stats['range_m']:.3f} m")
    print(f"Std Deviation:    {stats['stddev_cm']:.1f} cm")
    print(f"\nError:            {stats['error_cm']:+.1f} cm ({stats['error_percent']:+.1f}%)")

    if abs(stats['error_m']) > 0.001:  # More than 1mm error
        adjustment = calculate_antenna_delay_adjustment(stats['error_m'])
        print(f"\nRecommended Adjustment:")
        print(f"  Antenna delay adjustment: {adjustment:+d} time units")
        print(f"  (Add this to current antenna delay value)")

        if abs(stats['error_cm']) < 5.0:
            print(f"\n✓ Calibration within target (< 5 cm)")
        elif abs(stats['error_cm']) < 10.0:
            print(f"\n⚠ Calibration acceptable (< 10 cm)")
        else:
            print(f"\n✗ Calibration needs improvement (> 10 cm)")
    else:
        print(f"\n✓ Excellent calibration!")

    print("=" * 60 + "\n")


def save_json(stats: Dict, output_path: Path):
    """
    Save statistics to JSON file.
    """
    try:
        with open(output_path, 'w') as f:
            json.dump(stats, f, indent=2)
        print(f"Statistics saved to: {output_path}")
    except Exception as e:
        print(f"ERROR: Failed to save JSON: {e}", file=sys.stderr)


def save_csv(stats: Dict, output_path: Path):
    """
    Save statistics to CSV file.
    """
    try:
        with open(output_path, 'w', newline='') as f:
            writer = csv.writer(f)
            writer.writerow(['Metric', 'Value', 'Unit'])

            writer.writerow(['Samples', stats['num_samples'], 'count'])
            writer.writerow(['Actual Distance', f"{stats['actual_distance_m']:.3f}", 'm'])
            writer.writerow(['Mean Distance', f"{stats['mean_distance_m']:.3f}", 'm'])
            writer.writerow(['Median Distance', f"{stats['median_distance_m']:.3f}", 'm'])
            writer.writerow(['Std Deviation', f"{stats['stddev_cm']:.1f}", 'cm'])
            writer.writerow(['Min Distance', f"{stats['min_distance_m']:.3f}", 'm'])
            writer.writerow(['Max Distance', f"{stats['max_distance_m']:.3f}", 'm'])
            writer.writerow(['Range', f"{stats['range_m']:.3f}", 'm'])
            writer.writerow(['Error', f"{stats['error_cm']:.1f}", 'cm'])
            writer.writerow(['Error Percent', f"{stats['error_percent']:.1f}", '%'])

        print(f"Statistics saved to: {output_path}")
    except Exception as e:
        print(f"ERROR: Failed to save CSV: {e}", file=sys.stderr)


def plot_measurements(distances: List[float], stats: Dict, output_path: Optional[Path] = None):
    """
    Generate plots of measurements (requires matplotlib).
    """
    try:
        import matplotlib.pyplot as plt
        import numpy as np
    except ImportError:
        print("WARNING: matplotlib not available, skipping plots", file=sys.stderr)
        return

    fig, axes = plt.subplots(2, 2, figsize=(12, 10))

    # Plot 1: Time series
    ax1 = axes[0, 0]
    ax1.plot(distances, marker='o', markersize=2, linestyle='-', linewidth=0.5, alpha=0.7)
    ax1.axhline(y=stats['actual_distance_m'], color='g', linestyle='--', label='Actual')
    ax1.axhline(y=stats['mean_distance_m'], color='r', linestyle='--', label='Mean')
    ax1.set_xlabel('Sample Number')
    ax1.set_ylabel('Distance (m)')
    ax1.set_title('Distance Measurements Over Time')
    ax1.legend()
    ax1.grid(True, alpha=0.3)

    # Plot 2: Histogram
    ax2 = axes[0, 1]
    ax2.hist(distances, bins=30, alpha=0.7, edgecolor='black')
    ax2.axvline(x=stats['actual_distance_m'], color='g', linestyle='--', linewidth=2, label='Actual')
    ax2.axvline(x=stats['mean_distance_m'], color='r', linestyle='--', linewidth=2, label='Mean')
    ax2.set_xlabel('Distance (m)')
    ax2.set_ylabel('Frequency')
    ax2.set_title('Distribution of Measurements')
    ax2.legend()
    ax2.grid(True, alpha=0.3)

    # Plot 3: Error over time
    ax3 = axes[1, 0]
    errors = [(d - stats['actual_distance_m']) * 100 for d in distances]  # Convert to cm
    ax3.plot(errors, marker='o', markersize=2, linestyle='-', linewidth=0.5, alpha=0.7, color='orange')
    ax3.axhline(y=0, color='g', linestyle='--', label='Zero Error')
    ax3.axhline(y=stats['error_cm'], color='r', linestyle='--', label='Mean Error')
    ax3.set_xlabel('Sample Number')
    ax3.set_ylabel('Error (cm)')
    ax3.set_title('Measurement Error Over Time')
    ax3.legend()
    ax3.grid(True, alpha=0.3)

    # Plot 4: Statistics summary
    ax4 = axes[1, 1]
    ax4.axis('off')

    summary_text = f"""
Measurement Statistics

Samples: {stats['num_samples']}
Actual Distance: {stats['actual_distance_m']:.3f} m

Measured:
  Mean:   {stats['mean_distance_m']:.3f} m
  Median: {stats['median_distance_m']:.3f} m
  Std Dev: {stats['stddev_cm']:.1f} cm
  Min:    {stats['min_distance_m']:.3f} m
  Max:    {stats['max_distance_m']:.3f} m

Error:
  Absolute: {stats['error_cm']:+.1f} cm
  Percent:  {stats['error_percent']:+.1f} %

Antenna Delay Adjustment:
  {calculate_antenna_delay_adjustment(stats['error_m']):+d} time units
"""

    ax4.text(0.1, 0.5, summary_text, fontsize=10, family='monospace',
             verticalalignment='center')

    plt.tight_layout()

    if output_path:
        plt.savefig(output_path, dpi=150, bbox_inches='tight')
        print(f"Plot saved to: {output_path}")
    else:
        plt.show()

    plt.close()


def plot_validation_results(csv_path: Path, output_path: Optional[Path] = None):
    """
    Generate validation plots from multi-distance test results.
    """
    try:
        import matplotlib.pyplot as plt
        import numpy as np
    except ImportError:
        print("WARNING: matplotlib not available, skipping plots", file=sys.stderr)
        return

    # Read validation CSV
    actual_distances = []
    measured_distances = []
    errors = []
    stddevs = []

    try:
        with open(csv_path, 'r') as f:
            reader = csv.DictReader(f)
            for row in reader:
                actual_distances.append(float(row['Actual_Distance_m']))
                measured_distances.append(float(row['Measured_Distance_m']))
                errors.append(float(row['Error_cm']))
                stddevs.append(float(row['StdDev_cm']))
    except Exception as e:
        print(f"ERROR: Failed to read validation CSV: {e}", file=sys.stderr)
        return

    # Create plots
    fig, axes = plt.subplots(2, 2, figsize=(12, 10))

    # Plot 1: Actual vs Measured
    ax1 = axes[0, 0]
    ax1.plot(actual_distances, measured_distances, 'o-', markersize=8, label='Measured')
    ax1.plot(actual_distances, actual_distances, 'g--', linewidth=2, label='Ideal')
    ax1.set_xlabel('Actual Distance (m)')
    ax1.set_ylabel('Measured Distance (m)')
    ax1.set_title('Actual vs Measured Distance')
    ax1.legend()
    ax1.grid(True, alpha=0.3)

    # Plot 2: Error vs Distance
    ax2 = axes[0, 1]
    ax2.plot(actual_distances, errors, 'o-', markersize=8, color='red')
    ax2.axhline(y=0, color='g', linestyle='--', label='Zero Error')
    ax2.axhline(y=5, color='orange', linestyle=':', label='±5 cm')
    ax2.axhline(y=-5, color='orange', linestyle=':')
    ax2.axhline(y=10, color='red', linestyle=':', label='±10 cm')
    ax2.axhline(y=-10, color='red', linestyle=':')
    ax2.set_xlabel('Actual Distance (m)')
    ax2.set_ylabel('Error (cm)')
    ax2.set_title('Measurement Error vs Distance')
    ax2.legend()
    ax2.grid(True, alpha=0.3)

    # Plot 3: Standard Deviation vs Distance
    ax3 = axes[1, 0]
    ax3.plot(actual_distances, stddevs, 'o-', markersize=8, color='blue')
    ax3.set_xlabel('Actual Distance (m)')
    ax3.set_ylabel('Standard Deviation (cm)')
    ax3.set_title('Measurement Precision vs Distance')
    ax3.grid(True, alpha=0.3)

    # Plot 4: Error bars
    ax4 = axes[1, 1]
    x_pos = range(len(actual_distances))
    ax4.bar(x_pos, errors, yerr=stddevs, capsize=5, alpha=0.7, color='skyblue', edgecolor='black')
    ax4.axhline(y=0, color='g', linestyle='--')
    ax4.axhline(y=5, color='orange', linestyle=':')
    ax4.axhline(y=-5, color='orange', linestyle=':')
    ax4.axhline(y=10, color='red', linestyle=':')
    ax4.axhline(y=-10, color='red', linestyle=':')
    ax4.set_xticks(x_pos)
    ax4.set_xticklabels([f'{d:.1f}m' for d in actual_distances])
    ax4.set_ylabel('Error (cm)')
    ax4.set_title('Error with Standard Deviation')
    ax4.grid(True, alpha=0.3, axis='y')

    plt.tight_layout()

    if output_path:
        plt.savefig(output_path, dpi=150, bbox_inches='tight')
        print(f"Validation plot saved to: {output_path}")
    else:
        plt.show()

    plt.close()


def main():
    parser = argparse.ArgumentParser(
        description='Analyze DW1000 ranging measurements for calibration',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Analyze measurements and display statistics
  python3 analyze_measurements.py --input data.csv --actual-distance 1.0

  # Save results to JSON
  python3 analyze_measurements.py --input data.csv --actual-distance 1.0 --output-json results.json

  # Generate plots
  python3 analyze_measurements.py --input data.csv --actual-distance 1.0 --plot --output-plot plot.png

  # Validate multi-distance results
  python3 analyze_measurements.py --plot-validation validation_summary.csv
        """
    )

    parser.add_argument('--input', '-i', type=Path,
                        help='Input CSV file with measurements')
    parser.add_argument('--actual-distance', '-d', type=float,
                        help='Actual distance in meters')
    parser.add_argument('--output-json', '-j', type=Path,
                        help='Output JSON file for statistics')
    parser.add_argument('--output-csv', '-c', type=Path,
                        help='Output CSV file for statistics')
    parser.add_argument('--plot', action='store_true',
                        help='Generate plots (requires matplotlib)')
    parser.add_argument('--output-plot', '-p', type=Path,
                        help='Output file for plot (PNG)')
    parser.add_argument('--plot-validation', type=Path,
                        help='Generate validation plots from summary CSV')
    parser.add_argument('--quiet', '-q', action='store_true',
                        help='Suppress console output')

    args = parser.parse_args()

    # Validation plot mode
    if args.plot_validation:
        plot_validation_results(args.plot_validation, args.output_plot)
        return

    # Regular analysis mode
    if not args.input:
        parser.error('--input is required (unless using --plot-validation)')

    if args.actual_distance is None:
        parser.error('--actual-distance is required')

    # Parse data
    distances = parse_csv_data(args.input)

    if not distances:
        print(f"ERROR: No valid measurements found in {args.input}", file=sys.stderr)
        sys.exit(1)

    # Calculate statistics
    stats = calculate_statistics(distances, args.actual_distance)

    # Print statistics
    print_statistics(stats, args.quiet)

    # Save outputs
    if args.output_json:
        save_json(stats, args.output_json)

    if args.output_csv:
        save_csv(stats, args.output_csv)

    # Generate plots
    if args.plot or args.output_plot:
        plot_measurements(distances, stats, args.output_plot)


if __name__ == '__main__':
    main()
