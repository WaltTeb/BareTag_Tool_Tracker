import serial
import json
from Anchor import Anchor

def all_anchors_updated(anchors):
    for anchor in anchors:
        if not anchor.updated:
            return False
    return True

def trilaterate(anchors):
    x1 = anchors[0].x_coord
    x2 = anchors[1].x_coord
    x3 = anchors[2].x_coord

    y1 = anchors[0].y_coord
    y2 = anchors[1].y_coord
    y3 = anchors[2].y_coord

    r1 = anchors[0].get_dist()
    r2 = anchors[1].get_dist()
    r3 = anchors[2].get_dist()

    A = -2*x1 + 2*x2
    B = -2*y1 + 2*y2
    C = r1**2 - r2**2 - x1**2 + x2**2 - y1**2 + y2**2
    D = -2*x2 + 2*x3
    E = -2*y2 + 2*y3
    F = r2**2 - r3**2 - x2**2 + x3**2 - y2**2 + y3**2

    X = (C*E - F*B) / (E*A - B*D)
    Y = (C*D - A*F) / (B*D - A*E)

    return (X, Y)

try:
    with open('anchor_config.json') as config_file:
        data = json.load(config_file)

    lora_usb_port = data['lora_usb_port']

    anchor_list_json = data['anchors']
    anchor_list = []
    anchor_dict = {}

    for anchor in anchor_list_json:
        new_anchor = Anchor(anchor['anchor_id'], anchor['x_coord'], anchor['y_coord'])
        anchor_list.append(new_anchor)
        anchor_dict[new_anchor.id] = new_anchor


    with serial.Serial(f"/dev/{lora_usb_port}", 115200, timeout=1) as ser:
        while 1:
            line = ser.readline()
            if len(line) > 3:
                line_str = line.decode("utf-8")[:-2]
                if line_str[:4] == "+RCV":
                    recv_data = line_str.split(',')
                    recv_id = int(recv_data[0][5:])
                    recv_dist = float(recv_data[2])
                    anchor_dict[recv_id].update_dist(recv_dist)
            if all_anchors_updated(anchor_list):
                tag_location = trilaterate(anchor_list)
                print(tag_location)
    
except KeyboardInterrupt as e:
    print(f"Program quit with exception {e}")
