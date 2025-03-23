# Read content
html = open('./pixelit-webui-artifact/index.html', 'r')
o = html.read()
content = o.replace('\n', '')
html.close()

# Print the content of the file
print(content)

# HTML content to C++ string
content = "const char mainPage[] PROGMEM = R\"=====(" + content + ")=====\";"

# Write the content of the file
webuih = open('./include/Webinterface.h', 'w')
webuih.write(content)
webuih.close()

