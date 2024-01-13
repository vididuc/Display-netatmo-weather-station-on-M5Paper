import lnetatmo
import json

#############################################################################################
# Marcos 13.11.2023 Get netatmo data
# Place Config File im home directory: .netatmo.credentials
# https://github.com/philippelt/netatmo-api-python
# 
# 1 : Authenticate
authorization = lnetatmo.ClientAuth()

# 2 : Get devices list
weatherData = lnetatmo.WeatherStationData(authorization)

# 3 : Access most fresh data directly
#print ("Current temperature (inside/outside): %s / %s °C" %
#            ( weatherData.lastData()['indoor']['Temperature'],
#              weatherData.lastData()['outdoor']['Temperature'])
#)

#print ( weatherData.lastData())

print ("Current temperature (inside Wohnzimmer/outside Balkon): %s / %s °C" %
            ( weatherData.lastData()['Wohnzimmer']['Temperature'],
              weatherData.lastData()['Balkon']['Temperature'])
)

print (weatherData.lastData())

# convert float to string
# https://pythonexamples.org/python-convert-float-to-string/
temperature_indoor = str (weatherData.lastData()['Wohnzimmer']['Temperature'])
humidity_indoor = str (weatherData.lastData()['Wohnzimmer']['Humidity'])
last_measured_indoor = weatherData.lastData()['Wohnzimmer']['When']
temperature_outdoor = str (weatherData.lastData()['Balkon']['Temperature'])
humidity_outdoor = str (weatherData.lastData()['Balkon']['Humidity'])
last_measured_outdoor = weatherData.lastData()['Balkon']['When']

#############################################################################################


#############################################################################################
# Marcos 13.11.2023 create json doc
data = {
    'indoor' : 
        {
            'temperature' : temperature_indoor,
            'humidity' : humidity_indoor,
            'last_measured' : last_measured_indoor
        },
    'outdoor' :
        {
            'temperature' : temperature_outdoor,
            'humidity' : humidity_outdoor,
            'last_measured' : last_measured_outdoor
        }
}

# .dumps() as a string
json_string = json.dumps(data)
print(json_string)
#############################################################################################



# Marcos 13.11.2023
# write to file https://www.w3schools.com/python/python_file_write.asp
# write text to file
#f = open("/media/SynologyWebStation/temperature_file.txt", "w")
#f = open("/volume1/web/temperature_file.txt", "w")
#f.write(temperature_outdoor)
#f.close()

# read text from file
#f = open("/media/SynologyWebStation/temperature_file.txt", "r")
#print(f.read()) 
#f.close()

# Marcos 13.11.2023 write json to file 
#f = open("/media/SynologyWebStation/netatmo_lastdata.json", "w")
f = open("/volume1/web/netatmo_lastdata.json", "w")
f.write(json_string)
f.close()



