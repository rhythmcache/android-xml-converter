import random
import xml.etree.ElementTree as ET
import xml.dom.minidom as minidom
import sys

ELEMENT_NAMES = [
    'user', 'product', 'order', 'item', 'category', 'company', 'employee',
    'customer', 'address', 'contact', 'department', 'project', 'task',
    'config', 'setting', 'data', 'record', 'entry', 'node', 'info'
]

ATTR_NAMES = ['id', 'name', 'type', 'status', 'version', 'date', 'value', 'code']
ATTR_VALUES = ['active', 'pending', 'completed', 'draft', 'published', 'archived']

TEXT_SAMPLES = [
    'Sample text content with more data to increase file size significantly',
    'Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore',
    'Test data with extended information and additional details for larger XML generation',
    'Example value containing multiple words and longer descriptions to fill up space',
    'Random content generated with extra text to make the XML files bigger and more realistic',
    'Generated text with substantial length including various details and descriptive information',
    'Placeholder text extended to create larger file sizes for testing purposes with XML diff tools',
    'Demo text with comprehensive content spanning multiple lines and containing detailed information',
    'A1B2C3D4E5F6G7H8I9J0K1L2M3N4O5P6Q7R8S9T0U1V2W3X4Y5Z6',
    '2024-01-15T10:30:45Z with additional timestamp metadata and timezone information',
    'true/false values with extended boolean expressions and conditional logic descriptions',
    '42 is the answer to life, the universe, and everything according to Douglas Adams',
    '3.14159265358979323846264338327950288419716939937510',
    'Extended textual content designed specifically to bulk up XML file size for comprehensive testing'
]

def generate_element(depth=0, max_depth=4):
    """Generate a random XML element with children."""
    elem_name = random.choice(ELEMENT_NAMES)
    elem = ET.Element(elem_name)
    
    
    num_attrs = random.randint(1, 5)
    for _ in range(num_attrs):
        attr_name = random.choice(ATTR_NAMES)
        if random.random() > 0.5:
            attr_value = random.choice(ATTR_VALUES)
        else:
            attr_value = f"{random.randint(1, 100000)}"
        elem.set(attr_name, attr_value)
    
    
    if random.random() > 0.2:
        elem.text = random.choice(TEXT_SAMPLES)
    
    
    if depth < max_depth:
        num_children = random.randint(2, 8)
        for _ in range(num_children):
            child = generate_element(depth + 1, max_depth)
            elem.append(child)
    
    return elem

def prettify_xml(elem):
    """Return a pretty-printed XML string."""
    rough_string = ET.tostring(elem, encoding='unicode')
    reparsed = minidom.parseString(rough_string)
    return reparsed.toprettyxml(indent="  ")

def generate_xml_file(filename, max_depth=4):
    """Generate and save a random XML file."""
    root = generate_element(max_depth=max_depth)
    xml_string = prettify_xml(root)
    
    with open(filename, 'w', encoding='utf-8') as f:
        f.write(xml_string)
    
    import os
    size = os.path.getsize(filename)
    size_mb = size / (1024 * 1024)
    print(f"Generated: {filename} ({size_mb:.2f} MB)")

def generate_similar_xml(original_file, output_file, change_prob=0.3):
    """Generate a similar XML file with some random changes."""
    tree = ET.parse(original_file)
    root = tree.getroot()
    
    def modify_element(elem):
        """Randomly modify elements."""
        
        if elem.text and random.random() < change_prob:
            elem.text = random.choice(TEXT_SAMPLES)
        
        for attr in list(elem.attrib.keys()):
            if random.random() < change_prob:
                elem.set(attr, f"modified_{random.randint(1, 100000)}")
        
        
        if random.random() < change_prob / 2:
            elem.set(random.choice(ATTR_NAMES), random.choice(ATTR_VALUES))
        
        
        for child in elem:
            modify_element(child)
        
        if random.random() < change_prob / 3 and len(elem) < 10:
            new_child = generate_element(max_depth=2)
            elem.append(new_child)
    
    modify_element(root)
    xml_string = prettify_xml(root)
    
    with open(output_file, 'w', encoding='utf-8') as f:
        f.write(xml_string)
    
    import os
    size = os.path.getsize(output_file)
    size_mb = size / (1024 * 1024)
    print(f"Generated modified version: {output_file} ({size_mb:.2f} MB)")

def main():
    if len(sys.argv) < 2:
        print("Usage:")
        print("  Generate random XML files:")
        print("    python gen_xml.py random <num_files> [max_depth]")
        print("  Generate similar pairs for testing:")
        print("    python gen_xml.py pair <base_name> [change_probability]")
        sys.exit(1)
    
    mode = sys.argv[1]
    
    if mode == 'random':
        num_files = int(sys.argv[2]) if len(sys.argv) > 2 else 2
        max_depth = int(sys.argv[3]) if len(sys.argv) > 3 else 6  
        
        for i in range(num_files):
            filename = f"test_{i+1}.xml"
            generate_xml_file(filename, max_depth)
    
    elif mode == 'pair':
        base_name = sys.argv[2] if len(sys.argv) > 2 else "test"
        change_prob = float(sys.argv[3]) if len(sys.argv) > 3 else 0.3
        max_depth = int(sys.argv[4]) if len(sys.argv) > 4 else 6  
        
        file1 = f"{base_name}_1.xml"
        file2 = f"{base_name}_2.xml"
        
        generate_xml_file(file1)
        
        generate_similar_xml(file1, file2, change_prob)
        
        print(f"\nTest with: python diff.py {file1} {file2}")
    
    else:
        print(f"Unknown mode: {mode}")
        sys.exit(1)

if __name__ == "__main__":
    main()