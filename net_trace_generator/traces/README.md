# ICS Network Traces
Taken from https://github.com/tjcruz-dei/ICS_PCAPS

Recorded traces with PLC controlling a pump. 
- Clean trace is normal traffic (MODBUS/TCP) with approx 20 packets/s (packet length 61) 
- Synflood trace adds TCP traffic to a total of approx 200 packets/s (packet length 170)
- modbus queries add to a total of approx 500 packets/s (packet length 64)

### Related paper: 
Frazão, I. and Pedro Henriques Abreu and Tiago Cruz and Araújo, H. and Simões, P. , "Denial of Service Attacks: Detecting the frailties of machine learning algorithms in the Classification Process", in 13th International Conference on Critical Information Infrastructures Security (CRITIS 2018), ed. Springer, Kaunas, Lithuania, September 24-26, 2018, Springer series on Security and Cryptology , 2018. DOI: 10.1007/978-3-030-05849-4_19

# S7 Trace
- Traffic to a Siemens SIMATIC S7 (PLC) taken from https://www.netresec.com/?page=PCAP4SICS
- packet lengths = 80
- approx 10 packets/s with 110 packets/s peak at second 46