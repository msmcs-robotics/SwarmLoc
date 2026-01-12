/**
 * Configuration Header for Multi-Node Swarm Test
 *
 * Edit this file before uploading to each node
 * Change NODE_ID for each Arduino (1-5)
 */

#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// NODE CONFIGURATION - CHANGE THIS FOR EACH NODE
// ============================================================================

// Node ID: 1 (coordinator), 2-5 (mobile)
// IMPORTANT: Change this before uploading to each Arduino!
#define NODE_ID 1

// ============================================================================
// SWARM CONFIGURATION
// ============================================================================

// Number of nodes in the swarm (3-5)
#define MAX_NODES 5

// ============================================================================
// TDMA CONFIGURATION
// ============================================================================

// Enable TDMA slot management
#define ENABLE_TDMA true

// Slot duration (milliseconds)
// Adjust based on ranging speed and network size
#define SLOT_DURATION_MS 150

// Frame duration (total cycle time)
#define FRAME_DURATION_MS (MAX_NODES * SLOT_DURATION_MS)

// My TDMA slot (0-indexed)
// Node 1 (coordinator) doesn't use slots
// Node 2 = Slot 0, Node 3 = Slot 1, etc.
#define MY_SLOT_ID (NODE_ID - 2)

// Frame resync interval (milliseconds)
#define FRAME_RESYNC_MS 10000

// ============================================================================
// ADDRESSING CONFIGURATION
// ============================================================================

// Coordinator address (Node 1)
#define COORD_ADDRESS "82:17:5B:D5:A9:9A:E2:9A"

// Mobile node addresses (Nodes 2-5)
// The system automatically selects the right address based on NODE_ID
#define NODE2_ADDRESS "7D:00:22:EA:82:60:3B:9B"
#define NODE3_ADDRESS "7D:00:22:EA:82:60:3B:9C"
#define NODE4_ADDRESS "7D:00:22:EA:82:60:3B:9D"
#define NODE5_ADDRESS "7D:00:22:EA:82:60:3B:9E"

// Auto-select address based on NODE_ID
#if NODE_ID == 1
    #define MY_ADDRESS COORD_ADDRESS
#elif NODE_ID == 2
    #define MY_ADDRESS NODE2_ADDRESS
#elif NODE_ID == 3
    #define MY_ADDRESS NODE3_ADDRESS
#elif NODE_ID == 4
    #define MY_ADDRESS NODE4_ADDRESS
#elif NODE_ID == 5
    #define MY_ADDRESS NODE5_ADDRESS
#else
    #error "NODE_ID must be between 1 and 5"
#endif

// ============================================================================
// POSITION CONFIGURATION
// ============================================================================

// Enable position calculation for mobile nodes
#define ENABLE_POSITION_CALC true

// Position update rate (milliseconds)
#define POSITION_UPDATE_MS 500

// Default heights
#define COORDINATOR_HEIGHT 1.5  // meters
#define DEFAULT_TAG_HEIGHT 1.0  // meters

// ============================================================================
// RANGING CONFIGURATION
// ============================================================================

// Ranging update rate (Hz)
// Actual rate depends on TDMA slot allocation
#define RANGING_UPDATE_HZ 5

// Range timeout (milliseconds)
// After this time, a range is considered stale
#define RANGE_TIMEOUT_MS 5000

// ============================================================================
// COMMUNICATION CONFIGURATION
// ============================================================================

// Serial baud rate
#define SERIAL_BAUD 115200

// Heartbeat interval (milliseconds)
#define HEARTBEAT_MS 10000

// LED blink interval (milliseconds)
#define LED_BLINK_MS 500

// ============================================================================
// DEBUG FLAGS
// ============================================================================

// Enable TDMA debug output
#define DEBUG_TDMA false

// Enable position debug output
#define DEBUG_POSITION true

// Enable ranging debug output
#define DEBUG_RANGING false

// ============================================================================
// ADVANCED CONFIGURATION
// ============================================================================

// DW1000 mode
// Options: DW1000.MODE_LONGDATA_RANGE_LOWPOWER
//          DW1000.MODE_SHORTDATA_FAST_LOWPOWER
//          DW1000.MODE_LONGDATA_RANGE_ACCURACY (default)
//          DW1000.MODE_SHORTDATA_FAST_ACCURACY
#define DW1000_MODE DW1000.MODE_LONGDATA_RANGE_ACCURACY

// Maximum range (meters)
// Used for filtering outliers
#define MAX_VALID_RANGE 50.0

// Minimum range (meters)
// Used for filtering noise
#define MIN_VALID_RANGE 0.2

// ============================================================================
// ANCHOR POSITIONS (FOR TRILATERATION)
// ============================================================================

// These would normally be measured and entered after physical deployment
// For now, we only know the coordinator position

// Coordinator position (Node 1)
#define COORD_POS_X 0.0
#define COORD_POS_Y 0.0
#define COORD_POS_Z COORDINATOR_HEIGHT

// If you have additional fixed anchors, define them here:
// #define NODE2_POS_X 10.0
// #define NODE2_POS_Y 0.0
// #define NODE2_POS_Z COORDINATOR_HEIGHT

// ============================================================================
// MEMORY OPTIMIZATION
// ============================================================================

// Maximum number of simultaneous devices in DW1000Ranging library
// Default is 4, which fits in Arduino Uno RAM
// Reducing this saves memory but limits network size
#define MAX_DEVICES 4

// ============================================================================
// VALIDATION
// ============================================================================

#if NODE_ID < 1 || NODE_ID > MAX_NODES
    #error "NODE_ID must be between 1 and MAX_NODES"
#endif

#if MAX_NODES < 3 || MAX_NODES > 5
    #error "MAX_NODES must be between 3 and 5 for Arduino Uno"
#endif

#if SLOT_DURATION_MS < 50
    #error "SLOT_DURATION_MS too short - minimum 50ms"
#endif

#if SLOT_DURATION_MS > 500
    #warning "SLOT_DURATION_MS very long - may reduce update rate"
#endif

// ============================================================================
// CALCULATED VALUES
// ============================================================================

// Expected update rate per node (Hz)
#define EXPECTED_UPDATE_RATE_HZ (1000.0 / FRAME_DURATION_MS)

// Time per complete ranging cycle (ms)
#define CYCLE_TIME_MS FRAME_DURATION_MS

// ============================================================================
// CONFIGURATION SUMMARY
// ============================================================================

/*
 * Configuration Summary:
 *
 * Node ID: See NODE_ID define above
 * Total Nodes: MAX_NODES
 * TDMA Frame: FRAME_DURATION_MS ms
 * Slot Duration: SLOT_DURATION_MS ms
 * Expected Update Rate: ~EXPECTED_UPDATE_RATE_HZ Hz per node
 *
 * To configure a node:
 * 1. Set NODE_ID (1 for coordinator, 2-5 for mobile)
 * 2. Adjust MAX_NODES to match your swarm size
 * 3. Tune SLOT_DURATION_MS if needed (start with 150ms)
 * 4. Upload to Arduino
 * 5. Repeat for each node
 *
 * After deployment:
 * - Measure actual anchor positions
 * - Update ANCHOR_POSITIONS in node_firmware.ino
 * - Enable position calculation
 */

#endif // CONFIG_H
