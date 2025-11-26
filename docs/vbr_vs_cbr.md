# @page vbr_vs_cbr VBR vs CBR for NVENC

The NVENC rate control toggle allows switching between **variable bitrate (VBR)** and **constant bitrate (CBR)** modes. The table below summarizes common trade-offs to help decide which mode fits a given scenario.

| Mode | Pros | Cons | Best suited for |
| --- | --- | --- | --- |
| **CBR (Constant Bitrate)** | Predictable bandwidth usage that simplifies QoS and traffic shaping. <br> Works well with network equipment that expects a stable bitrate. <br> Minimizes rate spikes that could overload weaker links. | May waste bits on low-complexity scenes. <br> Can struggle to preserve detail during sudden motion or complex scenes unless the target bitrate is set generously. | Strict bandwidth caps, congested or inconsistent networks, or when integrating with services that penalize bitrate swings. |
| **VBR (Variable Bitrate)** | Allocates more bits to complex scenes and fewer to simple ones for better overall quality at the same average bitrate. <br> Reduces wasted bandwidth on static content. | Short-term bitrate spikes can exceed link capacity, causing potential buffering or packet loss if buffers are shallow. <br> Some older clients or middleware may mis-handle fluctuating bitrates. | Quality-focused streaming where the network has headroom or buffering (e.g., LAN, high-capacity uplinks), or workloads with frequent scene complexity changes. |

When using VBR, enable adaptive bitrate or ensure sufficient buffering to absorb spikes. When using CBR, choose a bitrate that leaves a small safety margin below the sustained network throughput.
