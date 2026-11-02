import React, { useState, useEffect, useRef } from 'react';
import { 
  Hash, 
  Cpu, 
  Terminal, 
  Server, 
  Sun, 
  Moon, 
  Activity, 
  Zap, 
  Database, 
  Layers, 
  Radio, 
  ShieldCheck, 
  ChevronDown, 
  Search, 
  Bell, 
  Pin, 
  UserCheck, 
  Play, 
  Pause, 
  RefreshCw,
  Send,
  Sliders,
  CheckCircle2
} from 'lucide-react';
import { IpcChannelType, PacketMessage, ProcessNode, ThreadMember } from './types';

const NODES: ProcessNode[] = [
  { id: 'node-local', name: 'macOS Core Node (Localhost)', icon: <Terminal className="w-5 h-5 text-white" />, activeProcesses: 8 },
  { id: 'node-cluster', name: 'POSIX Engine Worker', icon: <Server className="w-5 h-5 text-white" />, activeProcesses: 16 },
  { id: 'node-container', name: 'IPC Sandbox (Docker)', icon: <Layers className="w-5 h-5 text-white" />, activeProcesses: 4 },
];

const CHANNELS: { id: IpcChannelType; label: string; icon: React.ReactNode; throughput: string }[] = [
  { id: 'pipes', label: 'pipes', icon: <Radio className="w-4 h-4 mr-2" />, throughput: '1.4 GB/s' },
  { id: 'shared-memory', label: 'shared-memory', icon: <Zap className="w-4 h-4 mr-2 text-discord-yellow" />, throughput: '18.2 GB/s' },
  { id: 'message-queues', label: 'message-queues', icon: <Database className="w-4 h-4 mr-2" />, throughput: '420 MB/s' },
  { id: 'sockets', label: 'unix-domain-sockets', icon: <Activity className="w-4 h-4 mr-2" />, throughput: '4.8 GB/s' },
];

const INITIAL_THREADS: ThreadMember[] = [
  { pid: 10421, threadId: 1, role: 'Producer', status: 'High Load', cpuUsage: 89.4, memoryMb: 42.1 },
  { pid: 10422, threadId: 2, role: 'Consumer', status: 'Online', cpuUsage: 64.2, memoryMb: 112.8 },
  { pid: 10425, threadId: 3, role: 'Broker', status: 'Online', cpuUsage: 12.0, memoryMb: 18.4 },
  { pid: 10430, threadId: 4, role: 'Watcher', status: 'Idle', cpuUsage: 0.8, memoryMb: 8.2 },
];

export const App: React.FC = () => {
  const [darkMode, setDarkMode] = useState<boolean>(true);
  const [selectedNode, setSelectedNode] = useState<string>('node-local');
  const [activeChannel, setActiveChannel] = useState<IpcChannelType>('shared-memory');
  const [messages, setMessages] = useState<PacketMessage[]>([]);
  const [threads, setThreads] = useState<ThreadMember[]>(INITIAL_THREADS);
  const [isSimulating, setIsSimulating] = useState<boolean>(true);
  const [isConnectedToBackend, setIsConnectedToBackend] = useState<boolean>(false);
  const [selectedPayloadSize, setSelectedPayloadSize] = useState<string>('1MB');
  const [benchmarkStatus, setBenchmarkStatus] = useState<string>('Idle');
  const [selectedPacket, setSelectedPacket] = useState<PacketMessage | null>(null);

  const chatBottomRef = useRef<HTMLDivElement>(null);

  // Sync Dark Mode Class with HTML element
  useEffect(() => {
    if (darkMode) {
      document.documentElement.classList.add('dark');
    } else {
      document.documentElement.classList.remove('dark');
    }
  }, [darkMode]);

  // Connect to SSE Telemetry Gateway if available
  useEffect(() => {
    let eventSource: EventSource | null = null;
    try {
      eventSource = new EventSource('/api/events');
      eventSource.onopen = () => {
        setIsConnectedToBackend(true);
      };
      eventSource.onmessage = (e) => {
        try {
          const payload = JSON.parse(e.data);
          if (payload.event === 'packet' && payload.data) {
            setMessages((prev) => [payload.data, ...prev.slice(0, 75)]);
          }
        } catch (err) {
          console.error('Error parsing SSE event', err);
        }
      };
      eventSource.onerror = () => {
        setIsConnectedToBackend(false);
      };
    } catch {
      setIsConnectedToBackend(false);
    }

    return () => {
      if (eventSource) eventSource.close();
    };
  }, []);

  // Synthetic Local Generator (when backend SSE is disconnected or simulating)
  useEffect(() => {
    if (!isSimulating || isConnectedToBackend) return;

    const interval = setInterval(() => {
      const pids = [10421, 10422, 10425];
      const source = pids[Math.floor(Math.random() * pids.length)];
      const target = source === 10421 ? 10422 : 10421;
      
      const payloadMap = {
        'pipes': { size: '1 MB', bytes: 1048576, baseLat: 160 },
        'shared-memory': { size: '10 MB', bytes: 10485760, baseLat: 22 },
        'message-queues': { size: '8 KB', bytes: 8192, baseLat: 48 },
        'sockets': { size: '1 MB', bytes: 1048576, baseLat: 95 },
      };

      const spec = payloadMap[activeChannel];
      const jitter = (Math.random() * 14 - 7);
      const latency = Math.max(2, spec.baseLat + jitter);

      const newMsg: PacketMessage = {
        id: `pkt-${Date.now()}-${Math.floor(Math.random() * 1000)}`,
        timestamp: new Date().toLocaleTimeString(),
        sourcePid: source,
        targetPid: target,
        ipcType: activeChannel,
        payloadSize: spec.size,
        bytes: spec.bytes,
        latencyUs: parseFloat(latency.toFixed(1)),
        checksum: `0x${Math.floor(Math.random() * 0xFFFFFFFF).toString(16).toUpperCase()}`,
        status: 'verified'
      };

      setMessages((prev) => [newMsg, ...prev.slice(0, 75)]);
    }, 450);

    return () => clearInterval(interval);
  }, [activeChannel, isSimulating, isConnectedToBackend]);

  // Trigger Native C++ Benchmark via Gateway
  const triggerNativeBenchmark = async () => {
    setBenchmarkStatus('Executing Native C++ Run...');
    try {
      const mechCodeMap: Record<IpcChannelType, string> = {
        'pipes': 'pipes',
        'shared-memory': 'shm',
        'message-queues': 'mq',
        'sockets': 'sockets'
      };

      const resp = await fetch('/api/run-benchmark', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          mechanism: mechCodeMap[activeChannel],
          size: selectedPayloadSize,
          iterations: 15
        })
      });

      if (resp.ok) {
        setBenchmarkStatus('Streaming Telemetry');
        setTimeout(() => setBenchmarkStatus('Completed'), 2500);
      } else {
        setBenchmarkStatus('Backend Offline (Local Sim)');
      }
    } catch {
      setBenchmarkStatus('Backend Offline (Local Sim)');
    }
  };

  return (
    <div className={`flex h-screen w-screen overflow-hidden font-sans ${darkMode ? 'dark' : ''}`}>
      
      {/* 1. SERVER SIDEBAR (Far Left: System Nodes) */}
      <aside className="w-[72px] bg-discord-light-serverNav dark:bg-discord-dark-serverNav flex flex-col items-center py-3 space-y-2 select-none z-20 flex-shrink-0 border-r border-discord-light-divider dark:border-discord-dark-divider">
        
        {/* Discord Home / OS Root Symbol */}
        <button 
          title="Root Machine Monitor"
          className="relative group w-12 h-12 bg-discord-blurple rounded-3xl hover:rounded-2xl transition-all duration-200 flex items-center justify-center text-white shadow-md"
        >
          <Cpu className="w-6 h-6" />
          <div className="absolute left-0 w-1 bg-white rounded-r -translate-x-1 group-hover:translate-x-0 h-10 transition-all duration-200" />
        </button>

        <div className="w-8 h-[2px] bg-discord-light-divider dark:border-discord-dark-divider my-1" />

        {/* System Nodes List */}
        {NODES.map((node) => {
          const isActive = selectedNode === node.id;
          return (
            <div key={node.id} className="relative group w-full flex justify-center">
              <div className={`absolute left-0 w-1 bg-discord-light-textHeader dark:bg-discord-dark-textHeader rounded-r transition-all duration-200 ${
                isActive ? 'h-10' : 'h-2 group-hover:h-5'
              }`} />
              
              <button
                onClick={() => setSelectedNode(node.id)}
                title={`${node.name} (${node.activeProcesses} Threads)`}
                className={`w-12 h-12 rounded-3xl group-hover:rounded-2xl transition-all duration-200 flex items-center justify-center ${
                  isActive 
                    ? 'bg-discord-blurple text-white rounded-2xl' 
                    : 'bg-discord-light-hover dark:bg-discord-dark-channelNav text-discord-light-textNormal dark:text-discord-dark-textNormal hover:bg-discord-blurple hover:text-white'
                }`}
              >
                {node.icon}
              </button>
            </div>
          );
        })}

        {/* Dark / Light Theme Toggle Button */}
        <div className="mt-auto flex flex-col items-center space-y-2">
          <button
            onClick={() => setDarkMode(!darkMode)}
            title={`Switch to ${darkMode ? 'Light' : 'Dark'} Mode`}
            className="w-12 h-12 rounded-3xl hover:rounded-2xl bg-discord-light-hover dark:bg-discord-dark-channelNav text-discord-light-textHeader dark:text-discord-dark-textHeader hover:bg-discord-blurple hover:text-white transition-all flex items-center justify-center"
          >
            {darkMode ? <Sun className="w-5 h-5 text-discord-yellow" /> : <Moon className="w-5 h-5 text-discord-blurple" />}
          </button>
        </div>
      </aside>

      {/* 2. CHANNEL LIST (Inner Left: IPC Mechanisms) */}
      <nav className="w-60 bg-discord-light-channelNav dark:bg-discord-dark-channelNav flex flex-col flex-shrink-0 select-none border-r border-discord-light-divider dark:border-discord-dark-divider">
        {/* Node Header Dropdown */}
        <header className="h-12 border-b border-discord-light-divider dark:border-discord-dark-divider px-4 flex items-center justify-between font-semibold text-discord-light-textHeader dark:text-discord-dark-textHeader shadow-sm">
          <span className="truncate text-[15px]">OmniIPC Hub</span>
          <ChevronDown className="w-4 h-4 ml-1 opacity-70" />
        </header>

        {/* Category: Channels */}
        <div className="flex-1 overflow-y-auto px-2 py-3 space-y-[2px]">
          <div className="px-2 py-1 text-[11px] font-bold uppercase tracking-wider text-discord-light-textMuted dark:text-discord-dark-textMuted flex items-center justify-between">
            <span>IPC Primitives</span>
            <span className="text-[9px] bg-discord-blurple/20 text-discord-blurple px-1.5 py-0.5 rounded font-mono">POSIX</span>
          </div>

          {CHANNELS.map((channel) => {
            const isActive = activeChannel === channel.id;
            return (
              <button
                key={channel.id}
                onClick={() => setActiveChannel(channel.id)}
                className={`w-full flex items-center justify-between px-2 py-1.5 rounded-md text-[14px] font-medium transition-colors ${
                  isActive 
                    ? 'bg-discord-light-active dark:bg-discord-dark-active text-discord-light-textHeader dark:text-discord-dark-textHeader' 
                    : 'text-discord-light-textMuted dark:text-discord-dark-textMuted hover:bg-discord-light-hover dark:hover:bg-discord-dark-hover hover:text-discord-light-textNormal dark:hover:text-discord-dark-textNormal'
                }`}
              >
                <div className="flex items-center truncate">
                  <Hash className="w-4 h-4 mr-1.5 opacity-60 flex-shrink-0" />
                  <span className="truncate">{channel.label}</span>
                </div>
                <span className="text-[10px] opacity-75 font-mono ml-1">{channel.throughput}</span>
              </button>
            );
          })}

          <div className="pt-4 px-2 py-1 text-[11px] font-bold uppercase tracking-wider text-discord-light-textMuted dark:text-discord-dark-textMuted">
            Live Gateway Status
          </div>
          <div className="px-2 py-1.5 text-xs text-discord-light-textMuted dark:text-discord-dark-textMuted flex items-center space-x-2">
            <div className={`w-2 h-2 rounded-full ${isConnectedToBackend ? 'bg-discord-green' : 'bg-discord-yellow'}`} />
            <span>{isConnectedToBackend ? 'C++ Native SSE Gateway' : 'In-Memory Simulation'}</span>
          </div>

          {/* Quick Payload Selector */}
          <div className="pt-3 px-2">
            <span className="text-[10px] font-bold uppercase tracking-wider text-discord-light-textMuted dark:text-discord-dark-textMuted block mb-1">
              Payload Batch Size
            </span>
            <div className="grid grid-cols-4 gap-1 text-[11px] font-mono">
              {['1KB', '1MB', '10MB', '100MB'].map((size) => (
                <button
                  key={size}
                  onClick={() => setSelectedPayloadSize(size)}
                  className={`py-1 rounded text-center transition ${
                    selectedPayloadSize === size
                      ? 'bg-discord-blurple text-white font-bold'
                      : 'bg-discord-light-hover dark:bg-discord-dark-hover text-discord-light-textMuted dark:text-discord-dark-textMuted'
                  }`}
                >
                  {size}
                </button>
              ))}
            </div>
          </div>
        </div>

        {/* Footer: User/Process Profile Bar */}
        <footer className="h-[52px] bg-discord-light-serverNav/50 dark:bg-discord-dark-serverNav/50 px-2 flex items-center justify-between">
          <div className="flex items-center space-x-2">
            <div className="relative">
              <div className="w-8 h-8 rounded-full bg-discord-blurple flex items-center justify-center text-white text-xs font-bold font-mono">
                IPC
              </div>
              <div className="absolute bottom-0 right-0 w-3 h-3 bg-discord-green rounded-full border-2 border-discord-light-channelNav dark:border-discord-dark-channelNav" />
            </div>
            <div className="flex flex-col text-left leading-tight">
              <span className="text-xs font-bold text-discord-light-textHeader dark:text-discord-dark-textHeader">Benchmark Master</span>
              <span className="text-[10px] text-discord-light-textMuted dark:text-discord-dark-textMuted">Ready</span>
            </div>
          </div>
          <button 
            onClick={() => setIsSimulating(!isSimulating)}
            className={`p-1.5 rounded hover:bg-discord-light-hover dark:hover:bg-discord-dark-hover ${isSimulating ? 'text-discord-green' : 'text-discord-red'}`}
            title={isSimulating ? 'Pause Stream' : 'Resume Stream'}
          >
            {isSimulating ? <Activity className="w-4 h-4" /> : <Pause className="w-4 h-4" />}
          </button>
        </footer>
      </nav>

      {/* 3. MAIN CHAT AREA (Center: Packet Message Stream) */}
      <main className="flex-1 bg-discord-light-chatBg dark:bg-discord-dark-chatBg flex flex-col min-w-0">
        
        {/* Top Channel Bar */}
        <header className="h-12 border-b border-discord-light-divider dark:border-discord-dark-divider px-4 flex items-center justify-between flex-shrink-0 shadow-sm">
          <div className="flex items-center space-x-2">
            <Hash className="w-6 h-6 text-discord-light-textMuted dark:text-discord-dark-textMuted" />
            <span className="font-bold text-discord-light-textHeader dark:text-discord-dark-textHeader text-[15px]">{activeChannel}</span>
            <span className="text-xs text-discord-light-textMuted dark:text-discord-dark-textMuted pl-2 border-l border-discord-light-divider dark:border-discord-dark-divider">
              Hardware Monotonic Latency & Zero-Copy Framing Stream
            </span>
          </div>

          <div className="flex items-center space-x-3 text-discord-light-textMuted dark:text-discord-dark-textMuted">
            <button 
              onClick={triggerNativeBenchmark}
              className="flex items-center space-x-1.5 bg-discord-blurple text-white text-xs px-2.5 py-1.5 rounded font-medium hover:bg-opacity-90 transition shadow-sm"
              title="Run Native C++ Benchmark"
            >
              <Play className="w-3.5 h-3.5 fill-current" />
              <span>Run C++ Test</span>
            </button>
            <Pin className="w-5 h-5 cursor-pointer hover:text-discord-light-textNormal dark:hover:text-discord-dark-textNormal" />
            <Bell className="w-5 h-5 cursor-pointer hover:text-discord-light-textNormal dark:hover:text-discord-dark-textNormal" />
            <div className="relative">
              <input 
                type="text" 
                placeholder="Search Packets" 
                className="bg-discord-light-serverNav dark:bg-discord-dark-serverNav text-xs text-discord-light-textNormal dark:text-discord-dark-textNormal px-2 py-1 rounded pr-6 focus:outline-none focus:ring-1 focus:ring-discord-blurple"
              />
              <Search className="w-3.5 h-3.5 absolute right-1.5 top-1.5 opacity-60" />
            </div>
          </div>
        </header>

        {/* Message Packet Feed */}
        <div className="flex-1 overflow-y-auto p-4 space-y-3 flex flex-col-reverse">
          <div ref={chatBottomRef} />
          {messages.map((pkt) => (
            <div 
              key={pkt.id} 
              onClick={() => setSelectedPacket(pkt)}
              className="flex space-x-3 p-1.5 -mx-1.5 rounded hover:bg-discord-light-hover/40 dark:hover:bg-discord-dark-hover/30 transition-colors cursor-pointer group"
            >
              {/* Process Avatar */}
              <div className="w-10 h-10 rounded-full bg-gradient-to-tr from-discord-blurple to-indigo-600 flex items-center justify-center text-white font-mono text-xs font-bold flex-shrink-0 mt-0.5">
                {pkt.sourcePid.toString().slice(-2)}
              </div>

              {/* Message Content */}
              <div className="flex-1 min-w-0">
                <div className="flex items-baseline space-x-2">
                  <span className="text-[14px] font-semibold text-discord-light-textHeader dark:text-discord-dark-textHeader">
                    Process_{pkt.sourcePid}
                  </span>
                  <span className="text-[10px] px-1 bg-discord-blurple text-white rounded font-bold uppercase">PROD</span>
                  <span className="text-[11px] text-discord-light-textMuted dark:text-discord-dark-textMuted">
                    {pkt.timestamp}
                  </span>
                </div>

                {/* Packet Frame Embed Card */}
                <div className="mt-1 bg-discord-light-chatCard dark:bg-discord-dark-chatCard border border-discord-light-divider dark:border-discord-dark-divider rounded-md p-3 max-w-2xl text-xs space-y-2 shadow-sm group-hover:border-discord-blurple/50 transition">
                  <div className="flex justify-between items-center border-b border-discord-light-divider dark:border-discord-dark-divider pb-2">
                    <div className="flex items-center space-x-2 font-mono text-discord-blurple font-semibold">
                      <span>FRAME #{pkt.id.split('-')[2] || '0'}</span>
                      <span className="text-discord-light-textMuted dark:text-discord-dark-textMuted text-[10px]">({pkt.ipcType})</span>
                    </div>
                    <div className="flex items-center text-discord-green space-x-1 font-mono text-[11px]">
                      <ShieldCheck className="w-3.5 h-3.5" />
                      <span>{pkt.checksum}</span>
                    </div>
                  </div>

                  <div className="grid grid-cols-4 gap-2 pt-1 font-mono text-center">
                    <div className="bg-discord-light-serverNav dark:bg-discord-dark-serverNav p-1.5 rounded">
                      <div className="text-[10px] text-discord-light-textMuted dark:text-discord-dark-textMuted">SIZE</div>
                      <div className="font-semibold text-discord-light-textHeader dark:text-discord-dark-textHeader">{pkt.payloadSize}</div>
                    </div>
                    <div className="bg-discord-light-serverNav dark:bg-discord-dark-serverNav p-1.5 rounded">
                      <div className="text-[10px] text-discord-light-textMuted dark:text-discord-dark-textMuted">ROUTE</div>
                      <div className="font-semibold text-discord-light-textHeader dark:text-discord-dark-textHeader">{pkt.sourcePid} &rarr; {pkt.targetPid}</div>
                    </div>
                    <div className="bg-discord-light-serverNav dark:bg-discord-dark-serverNav p-1.5 rounded">
                      <div className="text-[10px] text-discord-light-textMuted dark:text-discord-dark-textMuted">ONE-WAY LAT</div>
                      <div className={`font-semibold ${pkt.latencyUs < 50 ? 'text-discord-green' : 'text-discord-yellow'}`}>
                        {pkt.latencyUs} &mu;s
                      </div>
                    </div>
                    <div className="bg-discord-light-serverNav dark:bg-discord-dark-serverNav p-1.5 rounded">
                      <div className="text-[10px] text-discord-light-textMuted dark:text-discord-dark-textMuted">INTEGRITY</div>
                      <div className="font-semibold text-discord-green uppercase text-[10px] mt-0.5">MATCHED</div>
                    </div>
                  </div>
                </div>
              </div>
            </div>
          ))}
        </div>

        {/* Input Bar: Trigger Manual Transmission Benchmark */}
        <div className="p-4 flex-shrink-0">
          <div className="bg-discord-light-hover dark:bg-discord-dark-channelNav rounded-lg px-4 py-2.5 flex items-center space-x-3">
            <span className="text-discord-light-textMuted dark:text-discord-dark-textMuted font-mono text-sm">+</span>
            <input 
              type="text" 
              placeholder={`Trigger native payload transmission across #${activeChannel} (${selectedPayloadSize})...`}
              className="bg-transparent flex-1 text-sm text-discord-light-textNormal dark:text-discord-dark-textNormal focus:outline-none placeholder-discord-light-textMuted dark:placeholder-discord-dark-textMuted"
              onKeyDown={(e) => {
                if (e.key === 'Enter') triggerNativeBenchmark();
              }}
            />
            <button 
              onClick={triggerNativeBenchmark}
              className="bg-discord-blurple text-white text-xs px-3 py-1.5 rounded font-medium hover:bg-opacity-90 transition flex items-center space-x-1"
            >
              <Send className="w-3.5 h-3.5" />
              <span>Transmit</span>
            </button>
          </div>
        </div>
      </main>

      {/* 4. MEMBERS LIST (Right: Active Process Threads) */}
      <aside className="w-60 bg-discord-light-memberNav dark:bg-discord-dark-memberNav flex flex-col flex-shrink-0 select-none border-l border-discord-light-divider dark:border-discord-dark-divider overflow-y-auto p-3 space-y-4">
        
        <div>
          <div className="text-[11px] font-bold uppercase tracking-wider text-discord-light-textMuted dark:text-discord-dark-textMuted mb-2">
            Active Process Threads — {threads.length}
          </div>

          <div className="space-y-1">
            {threads.map((t) => (
              <div 
                key={t.pid}
                className="flex items-center space-x-2.5 p-1.5 rounded-md hover:bg-discord-light-hover dark:hover:bg-discord-dark-hover cursor-pointer group transition-colors"
              >
                {/* Avatar with Status Bulb */}
                <div className="relative">
                  <div className="w-8 h-8 rounded-full bg-discord-light-serverNav dark:bg-discord-dark-serverNav flex items-center justify-center font-mono text-xs font-bold text-discord-light-textHeader dark:text-discord-dark-textHeader">
                    P{t.threadId}
                  </div>
                  <div className={`absolute bottom-0 right-0 w-2.5 h-2.5 rounded-full border-2 border-discord-light-memberNav dark:border-discord-dark-memberNav ${
                    t.status === 'High Load' ? 'bg-discord-red animate-ping' :
                    t.status === 'Online' ? 'bg-discord-green' : 'bg-discord-yellow'
                  }`} />
                  <div className={`absolute bottom-0 right-0 w-2.5 h-2.5 rounded-full border-2 border-discord-light-memberNav dark:border-discord-dark-memberNav ${
                    t.status === 'High Load' ? 'bg-discord-red' :
                    t.status === 'Online' ? 'bg-discord-green' : 'bg-discord-yellow'
                  }`} />
                </div>

                {/* Thread & Process Meta */}
                <div className="flex-1 min-w-0 leading-tight">
                  <div className="flex items-center justify-between">
                    <span className="text-xs font-semibold text-discord-light-textHeader dark:text-discord-dark-textHeader truncate">
                      PID_{t.pid}
                    </span>
                    <span className="text-[10px] font-mono text-discord-light-textMuted dark:text-discord-dark-textMuted">
                      {t.cpuUsage}%
                    </span>
                  </div>
                  <div className="flex items-center justify-between text-[10px] text-discord-light-textMuted dark:text-discord-dark-textMuted mt-0.5">
                    <span>{t.role}</span>
                    <span>{t.memoryMb} MB</span>
                  </div>
                </div>
              </div>
            ))}
          </div>
        </div>

        {/* Selected Packet Inspection Card */}
        {selectedPacket && (
          <div className="bg-discord-light-chatCard dark:bg-discord-dark-chatCard p-3 rounded-md border border-discord-blurple/40 text-[11px] space-y-2">
            <div className="font-semibold text-discord-light-textHeader dark:text-discord-dark-textHeader flex items-center justify-between">
              <span>Packet Inspector</span>
              <CheckCircle2 className="w-3.5 h-3.5 text-discord-green" />
            </div>
            <div className="font-mono text-[10px] text-discord-light-textMuted dark:text-discord-dark-textMuted space-y-1">
              <div>ID: {selectedPacket.id}</div>
              <div>Bytes: {selectedPacket.bytes.toLocaleString()} B</div>
              <div>Latency: {selectedPacket.latencyUs} &mu;s</div>
              <div>Checksum: {selectedPacket.checksum}</div>
            </div>
          </div>
        )}

        {/* System Load Sparkline / Summary Box */}
        <div className="mt-auto bg-discord-light-chatCard dark:bg-discord-dark-chatCard p-3 rounded-md border border-discord-light-divider dark:border-discord-dark-divider text-[11px] space-y-2">
          <div className="font-semibold text-discord-light-textHeader dark:text-discord-dark-textHeader flex items-center justify-between">
            <span>Kernel Scheduler Stats</span>
            <UserCheck className="w-3.5 h-3.5 text-discord-green" />
          </div>
          <div className="text-[10px] text-discord-light-textMuted dark:text-discord-dark-textMuted space-y-1">
            <div className="flex justify-between">
              <span>Voluntary Ctx Sw:</span>
              <span className="font-mono text-discord-light-textNormal dark:text-discord-dark-textNormal">1,402 /s</span>
            </div>
            <div className="flex justify-between">
              <span>Involuntary Ctx Sw:</span>
              <span className="font-mono text-discord-light-textNormal dark:text-discord-dark-textNormal">84 /s</span>
            </div>
            <div className="flex justify-between">
              <span>Page Faults (min):</span>
              <span className="font-mono text-discord-light-textNormal dark:text-discord-dark-textNormal">12 /s</span>
            </div>
          </div>
        </div>

      </aside>

    </div>
  );
};

export default App;
