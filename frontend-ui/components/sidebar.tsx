'use client';

import Link from 'next/link';
import { usePathname } from 'next/navigation';
import { cn } from '@/lib/utils';
import {
  Video,
  LayoutDashboard,
  Camera,
  Activity,
  Bell,
  Settings,
  Users,
  FileText,
  UserCircle,
  Webhook,
  ScrollText,
  ScanFace
} from 'lucide-react';
import { Badge } from '@/components/ui/badge';

const navigation = [
  { name: 'Dashboard', href: '/', icon: LayoutDashboard },
  { name: 'Cameras', href: '/cameras', icon: Camera },
  { name: 'Events', href: '/events', icon: Bell },
  { name: 'Face Detections', href: '/face-detections', icon: ScanFace },
  { name: 'Profiles', href: '/profiles', icon: UserCircle },
  { name: 'Logs', href: '/logs', icon: ScrollText },
  { name: 'Analytics', href: '/analytics', icon: Activity },
  { name: 'Watchlists', href: '/watchlists', icon: Users },
  { name: 'Webhooks', href: '/webhooks', icon: Webhook },
  { name: 'Reports', href: '/reports', icon: FileText },
  { name: 'Settings', href: '/settings', icon: Settings },
];

interface SidebarProps {
  wsConnected?: boolean;
}

export function Sidebar({ wsConnected }: SidebarProps = {}) {
  const pathname = usePathname();

  return (
    <div className="flex h-screen w-64 flex-col border-r bg-white dark:bg-slate-900">
      {/* Logo */}
      <div className="flex h-16 items-center gap-3 border-b px-6">
        <Video className="h-8 w-8 text-blue-600" />
        <div>
          <h1 className="text-lg font-bold bg-gradient-to-r from-blue-600 to-purple-600 bg-clip-text text-transparent">
            Camera System
          </h1>
          <p className="text-xs text-muted-foreground">Management</p>
        </div>
      </div>

      {/* Navigation */}
      <nav className="flex-1 space-y-1 overflow-y-auto p-4">
        {navigation.map((item) => {
          const isActive = pathname === item.href;
          return (
            <Link
              key={item.name}
              href={item.href}
              className={cn(
                'flex items-center gap-3 rounded-lg px-3 py-2 text-sm font-medium transition-all hover:bg-slate-100 dark:hover:bg-slate-800',
                isActive
                  ? 'bg-blue-50 text-blue-600 dark:bg-blue-900/20 dark:text-blue-400'
                  : 'text-slate-700 dark:text-slate-300'
              )}
            >
              <item.icon className="h-5 w-5" />
              {item.name}
            </Link>
          );
        })}
      </nav>

      {/* Footer */}
      <div className="border-t p-4">
        <div className="rounded-lg bg-gradient-to-r from-blue-50 to-purple-50 dark:from-blue-900/20 dark:to-purple-900/20 p-4">
          <h3 className="text-sm font-semibold text-slate-900 dark:text-white">
            System Status
          </h3>
          {wsConnected !== undefined && (
            <div className="mt-2">
              <Badge
                variant={wsConnected ? 'default' : 'secondary'}
                className={`gap-2 ${
                  wsConnected
                    ? 'bg-green-500 hover:bg-green-600'
                    : 'bg-gray-400'
                }`}
              >
                <span
                  className={`h-2 w-2 rounded-full ${
                    wsConnected ? 'bg-white animate-pulse' : 'bg-white'
                  }`}
                />
                {wsConnected ? 'Connected' : 'Disconnected'}
              </Badge>
            </div>
          )}
          {wsConnected === undefined && (
            <>
              <p className="mt-1 text-xs text-slate-600 dark:text-slate-400">
                All systems operational
              </p>
              <Badge className="mt-2 bg-green-500">Healthy</Badge>
            </>
          )}
        </div>
      </div>
    </div>
  );
}
