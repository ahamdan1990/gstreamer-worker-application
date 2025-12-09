'use client';

import Link from 'next/link';
import { usePathname } from 'next/navigation';
import { Badge } from '@/components/ui/badge';
import { Button } from '@/components/ui/button';
import {
  RefreshCw,
  Settings,
  User,
  Camera,
  Users,
  Shield,
  Webhook,
  Activity,
  LayoutDashboard,
  ScrollText
} from 'lucide-react';

interface HeaderProps {
  title?: string;
  description?: string;
  wsConnected?: boolean;
  onRefresh?: () => void;
}

const navItems = [
  { href: '/', label: 'Dashboard', icon: LayoutDashboard },
  { href: '/cameras', label: 'Cameras', icon: Camera },
  { href: '/profiles', label: 'Profiles', icon: Users },
  { href: '/watchlists', label: 'Watchlists', icon: Shield },
  { href: '/events', label: 'Events', icon: Activity },
  { href: '/logs', label: 'Logs', icon: ScrollText },
  { href: '/webhooks', label: 'Webhooks', icon: Webhook },
];

export function Header({ title, description, wsConnected, onRefresh }: HeaderProps) {
  const pathname = usePathname();

  return (
    <header className="sticky top-0 z-40 border-b bg-white backdrop-blur-sm dark:bg-slate-900">
      {/* Navigation Bar */}
      <div className="border-b border-gray-200 dark:border-gray-700">
        <div className="container mx-auto px-4">
          <div className="flex items-center justify-between h-16">
            {/* Logo and Nav Links */}
            <div className="flex items-center gap-8">
              <Link href="/" className="flex items-center gap-2">
                <div className="w-8 h-8 bg-blue-600 rounded-lg flex items-center justify-center">
                  <span className="text-white font-bold text-lg">N</span>
                </div>
                <span className="font-bold text-xl text-gray-900 dark:text-white hidden sm:block">
                  NexusVision
                </span>
              </Link>

              {/* Navigation Links */}
              <nav className="hidden md:flex items-center gap-1">
                {navItems.map((item) => {
                  const Icon = item.icon;
                  const isActive = pathname === item.href ||
                    (item.href !== '/' && pathname?.startsWith(item.href));

                  return (
                    <Link key={item.href} href={item.href}>
                      <Button
                        variant={isActive ? 'default' : 'ghost'}
                        size="sm"
                        className={`gap-2 ${
                          isActive
                            ? 'bg-blue-600 text-white hover:bg-blue-700'
                            : 'text-gray-600 hover:text-gray-900 dark:text-gray-400 dark:hover:text-white'
                        }`}
                      >
                        <Icon className="h-4 w-4" />
                        {item.label}
                      </Button>
                    </Link>
                  );
                })}
              </nav>
            </div>

            {/* Right Side Actions */}
            <div className="flex items-center gap-3">
              {wsConnected !== undefined && (
                <Badge
                  variant={wsConnected ? 'default' : 'secondary'}
                  className={`gap-1 ${
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
                  {wsConnected ? 'Live' : 'Offline'}
                </Badge>
              )}

              {onRefresh && (
                <Button onClick={onRefresh} variant="outline" size="sm">
                  <RefreshCw className="h-4 w-4 mr-2" />
                  Refresh
                </Button>
              )}

              <Button variant="ghost" size="icon">
                <Settings className="h-5 w-5" />
              </Button>

              <Button variant="ghost" size="icon">
                <User className="h-5 w-5" />
              </Button>
            </div>
          </div>
        </div>
      </div>

      {/* Page Title (optional, when title is provided) */}
      {title && (
        <div className="container mx-auto px-4 py-4">
          <h1 className="text-2xl font-bold text-gray-900 dark:text-white">{title}</h1>
          {description && (
            <p className="text-sm text-gray-600 dark:text-gray-400 mt-1">{description}</p>
          )}
        </div>
      )}
    </header>
  );
}
