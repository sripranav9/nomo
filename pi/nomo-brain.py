'''
                      -- NOMO v.1 --            
              Interactive Media Capstone 2025  

Authors          : Liza Datsenko and Sri Pranav Srivatsavai
Capstone Advisor : Prof. Nimrah Syed
Last updated     : 17 April 2025
Description      : Nomo is an interactive desk companion robot built to 
                   encourage healthier work habits through computer 
                   vision, gesture recognition, and playful nudges.

Special thanks to Prof. Michael Shiloh and Prof. Aya Riad
for their valuable contributions.

* NOTES
- The Work mode is referred to as the Study mode in some cases of the code.
- To run this .py script and intiate the robot, run the flask app (info below).
'''


import time
import cv2
import random
import mediapipe as mp
from mediapipe.tasks import python
from mediapipe.tasks.python import vision
from flask import Flask, Response
from picamera2 import Picamera2
import serial

# Flask App for Streaming
app = Flask(__name__)

# ** GLOBAL VARIABLES **
FACE_DETECTION_RESULT = None
GESTURE_RESULT_LIST = []
FRAME_COUNTER = 0  # For skipping frames

#Flags
study_mode_active = False
standby_mode_active = False
check_for_people = False
previous_command = None

break_first_nudge_sent = False
break_second_nudge_sent = False
study_first_nudge_sent = False
study_second_nudge_sent = False

# Timers
break_start_time = None
study_start_time = None
alone_timer_start = None  # Track when no one is detected

look_state = None
look_start_time = 0
LOOK_DURATION = 2  # how many seconds you want to spend "looking"

wave_last_time = None
WAVE_COOLDOWN = 3  # seconds

# Counters
closed_fist_counter = 0
thumb_up_counter = 0
previous_num_people = 0

COUNTER, FPS = 0, 0
START_TIME = time.time()
LOG_TIMER = time.time()  # Timer for reducing log frequency
# ** END OF GLOBAL VARIABLES **

# Serial variables
ser = serial.Serial("/dev/ttyACM0", 115200, timeout=1)
# Vairables for pyserial
ser.setDTR(False)
time.sleep(1)
ser.flushInput()
ser.setDTR(True)
time.sleep(2)

# Initialize MediaPipe utilities
mp_drawing = mp.solutions.drawing_utils
mp_drawing_styles = mp.solutions.drawing_styles

# Callback for Face Detection
def face_callback(result: vision.FaceDetectorResult, unused_output_image: mp.Image, timestamp_ms: int):
    global FACE_DETECTION_RESULT
    FACE_DETECTION_RESULT = result

# Callback for Gesture Recognition
def gesture_callback(result: vision.GestureRecognizerResult, unused_output_image: mp.Image, timestamp_ms: int):
    global GESTURE_RESULT_LIST
    GESTURE_RESULT_LIST.append(result)

# Initialize Picamera2
picam2 = Picamera2()
# picam2.configure(picam2.create_video_configuration(main={"size": (640, 480), "format": "RGB888"})) # Low resolution
picam2.configure(picam2.create_video_configuration(main={"size": (960, 540), "format": "RGB888"})) # Medium resolution
# picam2.configure(picam2.create_video_configuration(main={"size": (1280, 720), "format": "RGB888"})) # HD resolution
picam2.start()

# Function to send commands to Arduino only when there is a change
# def send_command(command):
#     global previous_command
#     if command != previous_command:
#         ser.write(command)
#         print(f"Sent to Arduino: {command}")
#         previous_command = command  # Update last sent command

def send_command(command):
    global previous_command
    if ser is None:
        print(f"WARNING: Serial port not available. Skipping command: {command}")
        return  # Skip sending if no connection

    if command != previous_command:
        ser.flushInput()  # Clear any buffered input before sending new data
        ser.write(command)  # Send command to Arduino
        ser.flush()  # Ensure the command is sent immediately
        print(f"Sent to Arduino: {command}")
        previous_command = command  # Update last sent command

# Function to generate frames with detection and logging
def generate_frames(face_model: str, gesture_model: str, skip_frames: int = 3):
    global FRAME_COUNTER, closed_fist_counter, alone_timer_start, FPS, COUNTER, START_TIME, LOG_TIMER, previous_num_people, thumb_up_counter, check_for_people, study_mode_active, standby_mode_active, break_start_time, study_start_time, break_first_nudge_sent, break_second_nudge_sent, study_first_nudge_sent, study_second_nudge_sent, previous_command, LOOK_DURATION, look_start_time, look_state, wave_last_time, WAVE_COOLDOWN

    # Initialize Face Detection
    face_base_options = python.BaseOptions(model_asset_path=face_model)
    face_options = vision.FaceDetectorOptions(
        base_options=face_base_options,
        running_mode=vision.RunningMode.LIVE_STREAM,
        min_detection_confidence=0.5,
        min_suppression_threshold=0.3,
        result_callback=face_callback
    )
    face_detector = vision.FaceDetector.create_from_options(face_options)

    # Initialize Gesture Recognition
    gesture_base_options = python.BaseOptions(model_asset_path=gesture_model)
    gesture_options = vision.GestureRecognizerOptions(
        base_options=gesture_base_options,
        running_mode=vision.RunningMode.LIVE_STREAM,
        num_hands=2,
        min_hand_detection_confidence=0.5,
        min_hand_presence_confidence=0.5,
        min_tracking_confidence=0.5,
        result_callback=gesture_callback
    )
    gesture_recognizer = vision.GestureRecognizer.create_from_options(gesture_options)

    # Variables
    update_interval = 1 # Interval variable for logging and actions in seconds, earlier #2
    last_gesture = None
    gesture_timer = time.time()  # Timer to manage gesture debounce duration

    while True:
        # Capture frame using Picamera2
        frame = picam2.capture_array()

        # Frame Skipping to reduce load
        FRAME_COUNTER += 1
        if FRAME_COUNTER % skip_frames != 0:
            continue

        rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb_frame)

        # Run Face Detection
        face_detector.detect_async(mp_image, time.time_ns() // 1_000_000)

        # Run Gesture Recognition
        gesture_recognizer.recognize_async(mp_image, time.time_ns() // 1_000_000)

        # Count Detected Faces
        num_people = len(FACE_DETECTION_RESULT.detections) if FACE_DETECTION_RESULT else 0

        # Detect Gestures
        gesture_detected = None
        if GESTURE_RESULT_LIST:
            for gesture_result in GESTURE_RESULT_LIST:
                if gesture_result.gestures:
                    current_gesture = gesture_result.gestures[0][0].category_name
                    if current_gesture != last_gesture and time.time() - gesture_timer > update_interval:
                        last_gesture = current_gesture
                        gesture_timer = time.time()  # Reset timer
                    gesture_detected = current_gesture
                    break
            GESTURE_RESULT_LIST.clear()

        
        # Interaction Logic (State-Based Logging)
        if time.time() - LOG_TIMER > update_interval:
            # Maintain current location if no new data is received
            try:
                # Uncomment
                if ser.in_waiting:
                    current_location = ser.readline().decode().strip()
                    print(f"Detected: {current_location}")
                else:
                    current_location = current_location if 'current_location' in locals() else "Nowhere"
                    # current_location = current_location if 'current_location' in locals() else "BLUE"
            except Exception as e:
                print(f"Serial read error: {e}")
                current_location = "Nowhere"
            
            # Determine current state based on number of people
            current_num_people = num_people
            
            # Detecting a change in people and reacting accordingly
            if current_num_people != previous_num_people:
                # 1) Takes precedence – people are detected
                if previous_num_people == 0 and current_num_people > 0 and not study_mode_active:
                    send_command(b'DETECTED') # Only works once because of the send_command logic
                    time.sleep(1.5)
                # 2) Be shy (Universal - is activated during study mode too)
                elif current_num_people >= 3 and not study_mode_active:
                    send_command(b'SHY')
                # 3) If expected people, normal position
                elif current_num_people < 3 and current_num_people > 0 and not study_mode_active:
                    send_command(b'NEUTRAL') # Command to Arduino to make the servo come back to normal.
                previous_num_people = current_num_people  # Update previous state
                
                # Reset flags because they were triggered when there were 0 people, but now there is atleast a positive change
                if current_num_people > 0:
                    alone_timer_start = None
                    check_for_people = False

            # If people are 0, then trigger the alone timer
            if current_num_people == 0:
                if not gesture_detected and alone_timer_start is None:
                    alone_timer_start = time.time()
                elif alone_timer_start is not None and time.time() - alone_timer_start > 15:
                    check_for_people = True
                    alone_timer_start = None # Reset alone timer - because action triggered
            else: 
                alone_timer_start = None
                check_for_people = False
                
            
            # Detecting Gestures for Study Mode Activation and Extension
            if gesture_detected == "Closed_Fist":
                if closed_fist_counter == 2:
                    if study_mode_active:
                        if current_location == "BLUE":
                            send_command(b'EXTEND_STUDY')
                            study_start_time = time.time()
                            #Reset break related flags
                            break_first_nudge_sent = False
                            break_second_nudge_sent = False
                            break_start_time = None  # Reset break tracking
                            break_location = None
                        
                        # Study mode already active, this means they are on a break
                        elif current_location in ("PURPLE", "YELLOW"):
                            send_command(b'EXTEND_BREAK')
                            break_start_time = time.time()
                            # Reset back to study related flags
                            study_first_nudge_sent = False
                            study_second_nudge_sent = False
                            study_start_time = None  # Reset study tracking
                    else:
                        if current_location == "BLUE":
                            send_command(b'STUDY')
                            study_mode_active = True
                            study_start_time = time.time()
                            
                            # Reset for a fresh study session
                            break_first_nudge_sent = False
                            break_second_nudge_sent = False
                            break_start_time = None
                            break_location = None
                            study_first_nudge_sent = False
                            study_second_nudge_sent = False
                        else:
                            send_command(b'MOVE_TO_DESK')
                    closed_fist_counter = 0
                else:
                    closed_fist_counter += 1
            
            # Study Mode Logic
            if study_mode_active:
                
                standby_mode_active = False
                
                # DURING STUDY
                if break_start_time is None:
                    elapsed_study_time = time.time() - study_start_time

                
                if elapsed_study_time > 40 and not break_first_nudge_sent: # 1 min for the capstone showcase
                    break_location = random.choice(["PURPLE", "YELLOW"])
                    # Nudge 1
                    send_command(f'BREAK_{break_location.upper()}'.encode())
                    break_first_nudge_sent = True # Mark the first nudge sent

                # Might be handled by Arduino. Pi will send a signal for second nudge check
                if elapsed_study_time > 70 and not break_second_nudge_sent:
                    send_command(b'CHECK_LOCATION')
                    time.sleep(1) # Request location and get it back
                    if current_location == "BLUE":
                        # Nudge 2 - Sound
                        send_command(b'SECOND_BREAK_NUDGE')
                        break_second_nudge_sent = True # Mark the first nudge sent
                    elif current_location not in ("PURPLE", "YELLOW", "BLUE"):
                        pass # the robot is being transported

                # Check if the break_location is reached - MIGHT BE HANDlED BY ARDUINO
                if break_first_nudge_sent and break_start_time is None: 
                    if current_location == break_location:
                        # print("Break location reached, starting break time")
                        break_start_time = time.time()
                        study_start_time = None # Reset study time during the break
                        # resetting from the last study session
                        # study_first_nudge_sent = False
                        # study_second_nudge_sent = False
        
                # DURING BREAK
                if break_start_time is not None and (time.time() - break_start_time > 20) and not study_first_nudge_sent: # 80 TBC
                    if current_location != "BLUE":
                        send_command(b'BACK_TO_STUDY') # Denote to be taken back
                    study_first_nudge_sent = True

                # work - requst location only once after its done
                if break_start_time is not None and (time.time() - break_start_time > 30) and not study_second_nudge_sent:  
                    send_command(b'CHECK_LOCATION')
                    study_second_nudge_sent = True
                    time.sleep(2)
                    if current_location == break_location:
                        # Nudge 2 - Sound
                        send_command(b'SECOND_STUDY_NUDGE')
                        break_second_nudge_sent = True # Mark the first nudge sent
                    elif current_location not in ("PURPLE", "YELLOW", "BLUE"):
                        pass # the robot is being transported

                # Check if the study_location is reached - Might be handled by the Arduino
                if study_first_nudge_sent:
                    if current_location == "BLUE": # Taken back to desk - all good
                        send_command(b'STUDY_RESTART')
                        
                        break_start_time = None # Reset break start time because
                        study_start_time = time.time()
                        # Reset the flags for next break
                        break_first_nudge_sent = False
                        break_second_nudge_sent = False
                        study_first_nudge_sent = False
                        study_second_nudge_sent = False
                        break_location = None # Resets break location
        
                # Quit Study Mode
                if gesture_detected == "Thumb_Up":
                    if thumb_up_counter == 2:
                        study_mode_active = False
                        print('Study Mode DEACTIVATED')
                        thumb_up_counter = 0
                    
                    else:
                        thumb_up_counter+=1
              
            # Standby Mode Logic
            if not study_mode_active:
                # Make sure we are only sending the standby mode once
                if not standby_mode_active:
                    send_command(b'STANDBY')
                    standby_mode_active = True
                    
                # WAVE
                if gesture_detected == "Open_Palm":
                    if wave_last_time is None or (time.time() - wave_last_time >= WAVE_COOLDOWN):
                        # To ingore flush and input on the send_command function
                        wave_last_time = time.time()
                        ser.write(b'WAVE')
                        print("Sent to Arduino: b'WAVE'")

                # ALONE
                # 1) If we just set check_for_people and we have no "look" in progress:
                if check_for_people and look_state is None:
                    # Start by looking left
                    send_command(b'LOOK_LEFT')
                    look_state = 'looking_left'
                    look_start_time = time.time()
                    check_for_people = False  # We’ve now started the "look" sequence, so we reset timer
                
                # 2) Currently looking left
                if look_state == 'looking_left':
                    if time.time() - look_start_time >= LOOK_DURATION:
                        # We’ve finished the left-look "wait"
                        if current_num_people > 0:
                            # send_command(b'NEUTRAL')
                            alone_timer_start = None
                            look_state = None  # Done
                        else:
                            # No people found, look right
                            send_command(b'LOOK_RIGHT')
                            look_state = 'looking_right'
                            look_start_time = time.time()
                
                # 3) If we are currently looking right...
                if look_state == 'looking_right':
                    if time.time() - look_start_time >= LOOK_DURATION:
                        if current_num_people > 0:
                            # send_command(b'NEUTRAL')
                            alone_timer_start = None
                        else:
                            send_command(b'NEUTRAL')
                            alone_timer_start = time.time()
                        look_state = None  # Done

            
            LOG_TIMER = time.time()
                    

        # Draw Bounding Boxes
        if FACE_DETECTION_RESULT:
            for detection in FACE_DETECTION_RESULT.detections:
                bbox = detection.bounding_box
                start_point = (int(bbox.origin_x), int(bbox.origin_y))
                end_point = (int(bbox.origin_x + bbox.width), int(bbox.origin_y + bbox.height))
                cv2.rectangle(frame, start_point, end_point, (0, 255, 0), 2)
                cv2.putText(frame, "Face", (start_point[0], start_point[1] - 10),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 0), 2)

        if gesture_detected:
            cv2.putText(frame, f"Gesture: {gesture_detected}", (10, 50),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 0), 2)

        # Convert to MJPEG Streaming Format
        _, buffer = cv2.imencode('.jpg', frame)
        yield (b'--frame\r\n'
               b'Content-Type: image/jpeg\r\n\r\n' + buffer.tobytes() + b'\r\n')

# Flask Route for Video Streaming
# Link: http://<ip>:<port>/nomo
@app.route('/nomo')
def nomo():
    return Response(generate_frames("/home/sripranav/Desktop/pi_camera/detector.tflite", "/home/sripranav/Desktop/pi_camera/gesture_recognizer.task", skip_frames=3), mimetype='multipart/x-mixed-replace; boundary=frame')

# Start Flask Server
if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, threaded=True)
