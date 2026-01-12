#!/usr/bin/env python3
"""
analyze_swarm_data.py - Swarm Test Data Analysis Tool

Project: SwarmLoc Multi-Node Swarm Test
Purpose: Analyze log files from multi-node swarm tests

Features:
- Parse log files from all nodes
- Generate ranging matrix
- Calculate network statistics
- Identify communication issues
- Create visualizations (optional)
- Export results to CSV

Usage:
    python3 analyze_swarm_data.py logs/
    python3 analyze_swarm_data.py logs/ --plot
    python3 analyze_swarm_data.py logs/ --export results.csv

Requirements:
    pip3 install matplotlib numpy (optional, for plots)
"""

import os
import sys
import re
import argparse
from datetime import datetime
from collections import defaultdict
import csv

# Optional imports
try:
    import numpy as np
    HAS_NUMPY = True
except ImportError:
    HAS_NUMPY = False
    print("WARNING: numpy not available. Some features disabled.")

try:
    import matplotlib.pyplot as plt
    import matplotlib.patches as patches
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False
    print("WARNING: matplotlib not available. Plotting disabled.")


class SwarmDataAnalyzer:
    """Analyzer for swarm test data."""

    def __init__(self, log_dir):
        self.log_dir = log_dir
        self.nodes = {}
        self.ranges = []
        self.events = []
        self.stats = defaultdict(lambda: {
            'range_count': 0,
            'error_count': 0,
            'discovery_count': 0,
            'disconnect_count': 0
        })

    def load_logs(self):
        """Load all log files from directory."""
        print(f"Loading logs from {self.log_dir}...")

        log_files = [f for f in os.listdir(self.log_dir) if f.endswith('.log')]

        if not log_files:
            print(f"ERROR: No .log files found in {self.log_dir}")
            return False

        print(f"Found {len(log_files)} log file(s)")

        for log_file in log_files:
            filepath = os.path.join(self.log_dir, log_file)
            node_id = self._extract_node_id(log_file)

            if node_id:
                print(f"  Loading Node {node_id}: {log_file}")
                self._parse_log_file(filepath, node_id)
            else:
                print(f"  WARNING: Could not extract node ID from {log_file}")

        return True

    def _extract_node_id(self, filename):
        """Extract node ID from filename (e.g., node_1_timestamp.log)."""
        match = re.search(r'node_(\d+)', filename)
        if match:
            return int(match.group(1))
        return None

    def _parse_log_file(self, filepath, node_id):
        """Parse a single log file."""
        with open(filepath, 'r') as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue

                # Parse CSV range data
                range_data = self._parse_range_line(line)
                if range_data:
                    range_data['source_node'] = node_id
                    self.ranges.append(range_data)
                    self.stats[node_id]['range_count'] += 1

                # Parse events
                if '[DISCOVER]' in line:
                    self.events.append({
                        'node': node_id,
                        'type': 'discovery',
                        'line': line
                    })
                    self.stats[node_id]['discovery_count'] += 1

                elif '[DISCONNECT]' in line:
                    self.events.append({
                        'node': node_id,
                        'type': 'disconnect',
                        'line': line
                    })
                    self.stats[node_id]['disconnect_count'] += 1

                elif 'ERROR' in line or 'FAIL' in line:
                    self.events.append({
                        'node': node_id,
                        'type': 'error',
                        'line': line
                    })
                    self.stats[node_id]['error_count'] += 1

    def _parse_range_line(self, line):
        """Parse CSV range line: timestamp,node_id,target_id,distance,rx_power"""
        try:
            # Skip non-CSV lines
            if not line[0].isdigit():
                return None

            parts = line.split(',')
            if len(parts) >= 5:
                return {
                    'timestamp': int(parts[0]),
                    'node_id': int(parts[1]),
                    'target_id': parts[2].strip(),
                    'distance': float(parts[3]),
                    'rx_power': float(parts[4])
                }
        except (ValueError, IndexError):
            pass
        return None

    def generate_report(self):
        """Generate analysis report."""
        print("\n" + "="*70)
        print("SWARM TEST ANALYSIS REPORT")
        print("="*70)

        # Overall statistics
        print("\nOVERALL STATISTICS")
        print("-" * 70)
        print(f"Total ranges recorded: {len(self.ranges)}")
        print(f"Total events: {len(self.events)}")
        print(f"Active nodes: {len(self.stats)}")

        # Per-node statistics
        print("\nPER-NODE STATISTICS")
        print("-" * 70)
        print(f"{'Node':<8} {'Ranges':<12} {'Discoveries':<14} {'Disconnects':<14} {'Errors':<8}")
        print("-" * 70)

        for node_id in sorted(self.stats.keys()):
            stats = self.stats[node_id]
            print(f"{node_id:<8} {stats['range_count']:<12} "
                  f"{stats['discovery_count']:<14} "
                  f"{stats['disconnect_count']:<14} "
                  f"{stats['error_count']:<8}")

        # Ranging matrix
        print("\nRANGING MATRIX")
        print("-" * 70)
        self._print_ranging_matrix()

        # Distance statistics
        if HAS_NUMPY:
            print("\nDISTANCE STATISTICS")
            print("-" * 70)
            self._print_distance_stats()

        # Network health
        print("\nNETWORK HEALTH")
        print("-" * 70)
        self._analyze_network_health()

        # Issues detected
        print("\nISSUES DETECTED")
        print("-" * 70)
        self._detect_issues()

        print("\n" + "="*70)

    def _print_ranging_matrix(self):
        """Print ranging matrix with statistics."""
        # Build matrix
        matrix = defaultdict(lambda: defaultdict(list))

        for range_data in self.ranges:
            src = range_data['node_id']
            tgt = range_data['target_id']
            dist = range_data['distance']
            matrix[src][tgt].append(dist)

        # Get all nodes
        all_nodes = set()
        for src in matrix:
            all_nodes.add(src)
            for tgt in matrix[src]:
                try:
                    # Try to convert hex target_id to int
                    tgt_int = int(tgt, 16) if isinstance(tgt, str) and '0x' not in tgt.lower() else int(tgt)
                    all_nodes.add(tgt_int)
                except:
                    pass

        if not all_nodes:
            print("No ranging data available")
            return

        sorted_nodes = sorted(all_nodes)

        # Print header
        print(f"{'From\\To':<10}", end='')
        for node in sorted_nodes:
            print(f"{node:<15}", end='')
        print()
        print("-" * (10 + 15 * len(sorted_nodes)))

        # Print matrix with average distances
        for src in sorted_nodes:
            print(f"{src:<10}", end='')
            for tgt in sorted_nodes:
                if src == tgt:
                    print(f"{'---':<15}", end='')
                else:
                    distances = matrix[src].get(str(tgt), [])
                    if not distances:
                        distances = matrix[src].get(hex(tgt), [])

                    if distances and HAS_NUMPY:
                        avg_dist = np.mean(distances)
                        std_dist = np.std(distances)
                        print(f"{avg_dist:.2f}±{std_dist:.2f}m"[:15] + " "*(15-min(15, len(f"{avg_dist:.2f}±{std_dist:.2f}m"))), end='')
                    elif distances:
                        avg_dist = sum(distances) / len(distances)
                        print(f"{avg_dist:.2f}m"[:15] + " "*(15-min(15, len(f"{avg_dist:.2f}m"))), end='')
                    else:
                        print(f"{'N/A':<15}", end='')
            print()

    def _print_distance_stats(self):
        """Print distance statistics using numpy."""
        if not self.ranges:
            print("No range data available")
            return

        distances = [r['distance'] for r in self.ranges]

        print(f"Count: {len(distances)}")
        print(f"Mean: {np.mean(distances):.3f} m")
        print(f"Std Dev: {np.std(distances):.3f} m")
        print(f"Min: {np.min(distances):.3f} m")
        print(f"Max: {np.max(distances):.3f} m")
        print(f"Median: {np.median(distances):.3f} m")

    def _analyze_network_health(self):
        """Analyze network health metrics."""
        total_ranges = sum(s['range_count'] for s in self.stats.values())
        total_errors = sum(s['error_count'] for s in self.stats.values())
        total_disconnects = sum(s['disconnect_count'] for s in self.stats.values())

        if total_ranges > 0:
            error_rate = (total_errors / total_ranges) * 100
            disconnect_rate = (total_disconnects / total_ranges) * 100

            print(f"Total successful ranges: {total_ranges}")
            print(f"Total errors: {total_errors}")
            print(f"Total disconnects: {total_disconnects}")
            print(f"Error rate: {error_rate:.2f}%")
            print(f"Disconnect rate: {disconnect_rate:.2f}%")

            # Health score
            if error_rate < 5 and disconnect_rate < 10:
                print("\n✓ Network health: GOOD")
            elif error_rate < 10 and disconnect_rate < 20:
                print("\n⚠ Network health: FAIR")
            else:
                print("\n✗ Network health: POOR")

    def _detect_issues(self):
        """Detect common issues in the data."""
        issues = []

        # Check for high error rates
        for node_id, stats in self.stats.items():
            if stats['range_count'] > 0:
                error_rate = (stats['error_count'] / stats['range_count']) * 100
                if error_rate > 10:
                    issues.append(f"Node {node_id}: High error rate ({error_rate:.1f}%)")

        # Check for missing nodes
        if len(self.stats) < 3:
            issues.append(f"Only {len(self.stats)} nodes detected (minimum 3 recommended)")

        # Check for asymmetric ranging
        matrix = defaultdict(lambda: defaultdict(int))
        for range_data in self.ranges:
            src = range_data['node_id']
            tgt = range_data['target_id']
            matrix[src][tgt] += 1

        for src in matrix:
            for tgt in matrix[src]:
                count_forward = matrix[src][tgt]
                count_reverse = matrix.get(int(tgt) if tgt.isdigit() else 0, {}).get(str(src), 0)
                if count_forward > 0 and count_reverse == 0:
                    issues.append(f"Asymmetric ranging: Node {src} → {tgt} works, but not reverse")

        # Check for distance outliers
        if HAS_NUMPY and self.ranges:
            distances = [r['distance'] for r in self.ranges]
            mean_dist = np.mean(distances)
            std_dist = np.std(distances)
            outliers = [r for r in self.ranges if abs(r['distance'] - mean_dist) > 3 * std_dist]
            if outliers:
                issues.append(f"Found {len(outliers)} distance outliers (>3σ from mean)")

        # Print issues
        if issues:
            for issue in issues:
                print(f"⚠ {issue}")
        else:
            print("✓ No major issues detected")

    def export_csv(self, output_file):
        """Export analysis to CSV."""
        print(f"\nExporting to {output_file}...")

        with open(output_file, 'w', newline='') as f:
            writer = csv.writer(f)

            # Write header
            writer.writerow(['timestamp', 'source_node', 'node_id', 'target_id',
                             'distance', 'rx_power'])

            # Write data
            for range_data in self.ranges:
                writer.writerow([
                    range_data['timestamp'],
                    range_data.get('source_node', 'N/A'),
                    range_data['node_id'],
                    range_data['target_id'],
                    range_data['distance'],
                    range_data['rx_power']
                ])

        print(f"✓ Exported {len(self.ranges)} range measurements")

    def plot_results(self):
        """Generate plots of the results."""
        if not HAS_MATPLOTLIB:
            print("ERROR: matplotlib not available. Cannot generate plots.")
            return

        print("\nGenerating plots...")

        # Create figure with subplots
        fig, axes = plt.subplots(2, 2, figsize=(14, 10))
        fig.suptitle('SwarmLoc Multi-Node Swarm Analysis', fontsize=16)

        # Plot 1: Distance histogram
        if self.ranges:
            distances = [r['distance'] for r in self.ranges]
            axes[0, 0].hist(distances, bins=30, edgecolor='black')
            axes[0, 0].set_title('Distance Distribution')
            axes[0, 0].set_xlabel('Distance (m)')
            axes[0, 0].set_ylabel('Frequency')
            axes[0, 0].grid(True, alpha=0.3)

        # Plot 2: Range count per node
        nodes = sorted(self.stats.keys())
        range_counts = [self.stats[n]['range_count'] for n in nodes]
        axes[0, 1].bar(nodes, range_counts, color='skyblue', edgecolor='black')
        axes[0, 1].set_title('Range Count per Node')
        axes[0, 1].set_xlabel('Node ID')
        axes[0, 1].set_ylabel('Range Count')
        axes[0, 1].grid(True, alpha=0.3)

        # Plot 3: Error rate per node
        error_rates = []
        for n in nodes:
            total = self.stats[n]['range_count']
            errors = self.stats[n]['error_count']
            rate = (errors / total * 100) if total > 0 else 0
            error_rates.append(rate)

        axes[1, 0].bar(nodes, error_rates, color='salmon', edgecolor='black')
        axes[1, 0].set_title('Error Rate per Node')
        axes[1, 0].set_xlabel('Node ID')
        axes[1, 0].set_ylabel('Error Rate (%)')
        axes[1, 0].grid(True, alpha=0.3)

        # Plot 4: RX Power distribution
        if self.ranges:
            rx_powers = [r['rx_power'] for r in self.ranges if r['rx_power'] != 0]
            if rx_powers:
                axes[1, 1].hist(rx_powers, bins=30, edgecolor='black', color='lightgreen')
                axes[1, 1].set_title('RX Power Distribution')
                axes[1, 1].set_xlabel('RX Power (dBm)')
                axes[1, 1].set_ylabel('Frequency')
                axes[1, 1].grid(True, alpha=0.3)

        plt.tight_layout()

        # Save plot
        output_file = 'swarm_analysis.png'
        plt.savefig(output_file, dpi=150)
        print(f"✓ Plot saved to {output_file}")

        # Show plot
        plt.show()


def main():
    parser = argparse.ArgumentParser(
        description='Analyze SwarmLoc multi-node test data',
        formatter_class=argparse.RawDescriptionHelpFormatter
    )

    parser.add_argument('log_dir', help='Directory containing log files')
    parser.add_argument('--export', metavar='FILE', help='Export results to CSV')
    parser.add_argument('--plot', action='store_true', help='Generate plots (requires matplotlib)')

    args = parser.parse_args()

    # Check log directory
    if not os.path.isdir(args.log_dir):
        print(f"ERROR: {args.log_dir} is not a directory")
        sys.exit(1)

    # Create analyzer
    analyzer = SwarmDataAnalyzer(args.log_dir)

    # Load logs
    if not analyzer.load_logs():
        sys.exit(1)

    # Generate report
    analyzer.generate_report()

    # Export if requested
    if args.export:
        analyzer.export_csv(args.export)

    # Plot if requested
    if args.plot:
        analyzer.plot_results()


if __name__ == '__main__':
    main()
