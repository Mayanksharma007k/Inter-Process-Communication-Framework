import React from 'react';

export type IpcChannelType = 'pipes' | 'shared-memory' | 'message-queues' | 'sockets';

export interface PacketMessage {
  id: string;
  timestamp: string;
  sourcePid: number;
  targetPid: number;
  ipcType: IpcChannelType;
  payloadSize: string;
  bytes: number;
  latencyUs: number;
  checksum: string;
  status: 'verified' | 'dropped' | 'corrupted';
}

export interface ProcessNode {
  id: string;
  name: string;
  icon: React.ReactNode;
  activeProcesses: number;
}

export interface ThreadMember {
  pid: number;
  threadId: number;
  role: 'Producer' | 'Consumer' | 'Broker' | 'Watcher';
  status: 'Online' | 'Idle' | 'High Load';
  cpuUsage: number;
  memoryMb: number;
}
