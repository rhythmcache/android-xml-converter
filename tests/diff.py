#!/usr/bin/env python3
import sys
import xml.etree.ElementTree as ET

def normalize_text(text):
    """Normalize text by stripping whitespace."""
    return text.strip() if text else ""

def compare_elements(elem1, elem2, path=""):
    """Recursively compare two XML elements."""
    diffs = []
    
    if elem1.tag != elem2.tag:
        diffs.append(f"{path}: Tag differs - '{elem1.tag}' vs '{elem2.tag}'")
        return diffs
    
    current_path = f"{path}/{elem1.tag}"
    
    attrs1 = elem1.attrib
    attrs2 = elem2.attrib
    
    all_attrs = set(attrs1.keys()) | set(attrs2.keys())
    for attr in sorted(all_attrs):
        val1 = attrs1.get(attr)
        val2 = attrs2.get(attr)
        if val1 != val2:
            diffs.append(f"{current_path}[@{attr}]: '{val1}' vs '{val2}'")
    
    text1 = normalize_text(elem1.text)
    text2 = normalize_text(elem2.text)
    if text1 != text2:
        diffs.append(f"{current_path}/text(): '{text1}' vs '{text2}'")
    

    tail1 = normalize_text(elem1.tail)
    tail2 = normalize_text(elem2.tail)
    if tail1 != tail2:
        diffs.append(f"{current_path}/tail(): '{tail1}' vs '{tail2}'")
    

    children1 = list(elem1)
    children2 = list(elem2)
    
    if len(children1) != len(children2):
        diffs.append(f"{current_path}: Different number of children - {len(children1)} vs {len(children2)}")
    
    for i, (child1, child2) in enumerate(zip(children1, children2)):
        diffs.extend(compare_elements(child1, child2, current_path))
    
    if len(children1) > len(children2):
        for i in range(len(children2), len(children1)):
            diffs.append(f"{current_path}: Extra element in file1 - {children1[i].tag}")
    elif len(children2) > len(children1):
        for i in range(len(children1), len(children2)):
            diffs.append(f"{current_path}: Extra element in file2 - {children2[i].tag}")
    
    return diffs

def main():
    if len(sys.argv) != 3:
        print("Usage: python diff.py <file1.xml> <file2.xml>")
        sys.exit(1)
    
    file1, file2 = sys.argv[1], sys.argv[2]
    
    try:
        tree1 = ET.parse(file1)
        tree2 = ET.parse(file2)
        root1 = tree1.getroot()
        root2 = tree2.getroot()
        
        diffs = compare_elements(root1, root2)
        
        if not diffs:
            print("- Files are semantically identical")
        else:
            print(f"- Found {len(diffs)} difference(s):\n")
            for diff in diffs:
                print(f"  • {diff}")
        
    except ET.ParseError as e:
        print(f"Error parsing XML: {e}")
        sys.exit(1)
    except FileNotFoundError as e:
        print(f"File not found: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()