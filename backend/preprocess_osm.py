import xml.etree.ElementTree as ET
import math
import sqlite3

# Function to calculate tile coordinates from latitude and longitude
def lat_lon_to_tile(lat, lon, zoom):
    lat_rad = math.radians(lat)
    n = 2.0 ** zoom
    xtile = int((lon + 180.0) / 360.0 * n)
    ytile = int((1.0 - math.log(math.tan(lat_rad) + (1 / math.cos(lat_rad))) / math.pi) / 2.0 * n)
    return (xtile, ytile)

# Function to preprocess OSM XML data and store nodes, tags, and ways in a database
def preprocess_osm_data(osm_file_path, db_path):
    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()
    cursor.execute('''CREATE TABLE IF NOT EXISTS nodes (
                        id INTEGER PRIMARY KEY, 
                        lat REAL, 
                        lon REAL, 
                        version INTEGER, 
                        timestamp TEXT, 
                        changeset INTEGER, 
                        uid INTEGER, 
                        user TEXT,
                        way_id INTEGER)''')
    cursor.execute('''CREATE TABLE IF NOT EXISTS tags (
                        node_id INTEGER, 
                        k TEXT, 
                        v TEXT,
                        FOREIGN KEY(node_id) REFERENCES nodes(id))''')
    cursor.execute('''CREATE TABLE IF NOT EXISTS ways (
                        id INTEGER PRIMARY KEY,
                        min_lat REAL,
                        max_lat REAL,
                        min_lon REAL,
                        max_lon REAL)''')
    cursor.execute('''CREATE TABLE IF NOT EXISTS node_tags (
                        node_id INTEGER, 
                        k TEXT, 
                        v TEXT,
                        FOREIGN KEY(node_id) REFERENCES nodes(id))''')
    cursor.execute('''CREATE TABLE IF NOT EXISTS way_nodes (
                        way_id INTEGER, 
                        node_id INTEGER,
                        FOREIGN KEY(way_id) REFERENCES ways(id),
                        FOREIGN KEY(node_id) REFERENCES nodes(id))''')
    cursor.execute('''CREATE TABLE IF NOT EXISTS way_tags (
                        way_id INTEGER, 
                        k TEXT, 
                        v TEXT,
                        FOREIGN KEY(way_id) REFERENCES ways(id))''')
    
    context = ET.iterparse(osm_file_path, events=("start", "end"))
    context = iter(context)
    event, root = next(context)
    
    for event, elem in context:
        if event == "end" and elem.tag == "node":
            node_id = int(elem.get('id'))
            lat = float(elem.get('lat'))
            lon = float(elem.get('lon'))
            version = int(elem.get('version'))
            timestamp = elem.get('timestamp')
            changeset = int(elem.get('changeset'))
            uid = int(elem.get('uid'))
            user = elem.get('user')
            cursor.execute('''INSERT INTO nodes (id, lat, lon, version, timestamp, changeset, uid, user, way_id) 
                              VALUES (?, ?, ?, ?, ?, ?, ?, ?, NULL)''', 
                           (node_id, lat, lon, version, timestamp, changeset, uid, user))
            for tag in elem.findall('tag'):
                k = tag.get('k')
                v = tag.get('v')
                cursor.execute('''INSERT INTO node_tags (node_id, k, v) 
                                  VALUES (?, ?, ?)''', 
                               (node_id, k, v))
            root.clear()
        elif event == "end" and elem.tag == "way":
            way_id = int(elem.get('id'))
            node_ids = [int(nd.get('ref')) for nd in elem.findall('nd')]

            # Fetch latitudes and longitudes of nodes
            cursor.execute('SELECT lat, lon FROM nodes WHERE id IN ({})'.format(','.join('?'*len(node_ids))), node_ids)
            lat_lons = cursor.fetchall()
            latitudes = [lat for lat, _ in lat_lons]
            longitudes = [lon for _, lon in lat_lons]

            if latitudes and longitudes:
                min_lat = min(latitudes)
                max_lat = max(latitudes)
                min_lon = min(longitudes)
                max_lon = max(longitudes)
                cursor.execute('''INSERT INTO ways (id, min_lat, max_lat, min_lon, max_lon) 
                                  VALUES (?, ?, ?, ?, ?)''', (way_id, min_lat, max_lat, min_lon, max_lon))
            
            # Insert way_nodes
            for node_id in node_ids:
                cursor.execute('''INSERT INTO way_nodes (way_id, node_id) 
                                  VALUES (?, ?)''', (way_id, node_id))
            # Insert way_tags
            for tag in elem.findall('tag'):
                k = tag.get('k')
                v = tag.get('v')
                cursor.execute('''INSERT INTO way_tags (way_id, k, v) 
                                  VALUES (?, ?, ?)''', (way_id, k, v))
            root.clear()
    
    # Create indexes
    cursor.execute('CREATE INDEX IF NOT EXISTS idx_nodes_lat_lon ON nodes(lat, lon)')
    cursor.execute('CREATE INDEX IF NOT EXISTS idx_ways_bbox ON ways(min_lat, max_lat, min_lon, max_lon)')
    
    conn.commit()
    conn.close()

def test_preprocess_osm_data():
    db_path = r'E:\BaiduSyncdisk\Code Projects\PyQt Projects\Data Structure Project\backend\data\map.db'
    
    # Connect to the database and verify the data
    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()
    
    # Test 1: Check if nodes table is created and populated
    cursor.execute("SELECT COUNT(*) FROM nodes")
    node_count = cursor.fetchone()[0]
    assert node_count > 0, "Nodes table should have entries"
    
    # Test 2: Check if tags table is created and populated
    cursor.execute("SELECT COUNT(*) FROM tags")
    tag_count = cursor.fetchone()[0]
    assert tag_count > 0, "Tags table should have entries"
    
    # Test 3: Verify a specific node's data
    cursor.execute("SELECT * FROM nodes WHERE id = 30198449")
    node = cursor.fetchone()
    assert node is not None, "Node with id 30198449 should exist"
    assert node[1] == 31.2791117, "Latitude should match"
    assert node[2] == 121.3039889, "Longitude should match"
    
    # Test 4: Verify a specific tag's data
    cursor.execute("SELECT * FROM tags WHERE node_id = 30198449")
    tags = cursor.fetchall()
    assert len(tags) > 0, "Tags for node 30198449 should exist"
    
    conn.close()
    print("All tests passed!")

# Run the tests
if __name__ == "__main__":
    test_preprocess_osm_data()
