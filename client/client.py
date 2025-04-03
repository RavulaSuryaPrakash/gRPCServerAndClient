import csv
import grpc
import data_transfer_pb2
import data_transfer_pb2_grpc

def parse_date(date_str):
    """Convert 'MM/DD/YYYY' to an integer YYYYMMDD."""
    if not date_str:
        return None
    date_str = date_str.replace('/', '')
    if len(date_str) != 8:
        return None
    # Rearranging MMDDYYYY -> YYYYMMDD:
    year = date_str[4:]
    month = date_str[0:2]
    day = date_str[2:4]
    return int(year + month + day)

def parse_time(time_str):
    """Convert 'HH:MM' to an integer HHMM."""
    if not time_str:
        return None
    parts = time_str.split(':')
    if len(parts) == 2 and len(parts[0]) == 1:
        time_str = '0' + time_str
    time_str = time_str.replace(':', '')
    if not (3 <= len(time_str) <= 4):
        return None
    return int(time_str)

def safe_int(value):
    try:
        return int(value)
    except:
        return 0

def stream_records(csv_file):
    """Generator function that reads the CSV file and yields CollisionRecordMsg objects."""
    with open(csv_file, newline='') as csvfile:
        reader = csv.DictReader(csvfile)
        for row in reader:
            date_int = parse_date(row.get('CRASH DATE', ''))
            time_int = parse_time(row.get('CRASH TIME', ''))
            if date_int is None or time_int is None:
                print("Skipping invalid date/time:", row)
                continue
            record_msg = data_transfer_pb2.CollisionRecordMsg(
                crash_date = date_int,
                crash_time = time_int,
                persons_injured = safe_int(row.get('NUMBER OF PERSONS INJURED', '0')),
                persons_killed  = safe_int(row.get('NUMBER OF PERSONS KILLED', '0')),
                pedestrians_injured = safe_int(row.get('NUMBER OF PEDESTRIANS INJURED', '0')),
                pedestrians_killed  = safe_int(row.get('NUMBER OF PEDESTRIANS KILLED', '0')),
                cyclists_injured    = safe_int(row.get('NUMBER OF CYCLIST INJURED', '0')),
                cyclists_killed     = safe_int(row.get('NUMBER OF CYCLIST KILLED', '0')),
                motorists_injured    = safe_int(row.get('NUMBER OF MOTORIST INJURED', '0')),
                motorists_killed     = safe_int(row.get('NUMBER OF MOTORIST KILLED', '0'))
            )
            yield record_msg

def run():
    # Create a channel to Processor A (Node A) at localhost:50051.
    channel = grpc.insecure_channel('localhost:50051')
    stub = data_transfer_pb2_grpc.DataTransferStub(channel)
    
    csv_file = 'data.csv'  # Ensure this path is correct.
    # Initiate the streaming RPC using our generator.
    response = stub.StreamData(stream_records(csv_file))
    
    print("Response from server:", response.message)

if __name__ == '__main__':
    run()
