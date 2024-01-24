# Requirements

## Problems/Things to change
### GetDesignID should not be part of the middleware but part of the skeleton

### Reading two times the output


## What should the code in here do
The idea is that I can validate the results of my HW-accelerator / network. 
Therefore, I want a C-runtime image that I can build with the HW-accelerator matching stub/skeleton. 

## Process
The validation should be done the following:

**Elastic-ai.creator Python Environment**
1. Build your model + train
2. Generate HW-Accelerator + matching Skeleton
3. Generate binfile

**Elastic-ai.runtime**
4. Generate matching Stub
5. Build C-Runtime

**Elastic-ai.cloud**
6. Start your Cloud application incl. MQTT Broker (unfortunatly very unspecific)

**Elastic-ai.enV5**
7. Flash the RP2040 over USB **sad noises :(**
8. enV5-Connects via MQTT to the broker
9. enV5 Subscribes to "compute-data"-topic to receive data
10. Flash the binfile via HTTP **happy :)**

**Send Data via MQTT**
11. Connect with a MQTT Client
12. Subscribe "computed-result"-topic
12. Send data to "compute-data"-topic
13. Wait till results are received via "computed-result" topic
14. Save the results

**Elastic-ai.creator Python Environment**
15. Check if the results match the software implementation
16. **PROFIT**



