'use client';

import { Badge } from '@/components/ui/badge';
import { Button } from '@/components/ui/button';
import { RefreshCw, Settings, User } from 'lucide-react';

interface HeaderProps {
  title: string;
  description?: string;
  wsConnected?: boolean;
  onRefresh?: () => void;
}

export function Header({ title, description, wsConnected, onRefresh }: HeaderProps) {
  return (
    <header className="sticky top-0 z-40 border-b bg-white/80 backdrop-blur-sm dark:bg-slate-900/80">
      <div className="flex h-16 items-center justify-between px-6">
        <div>
          <h1 className="text-2xl font-bold text-slate-900 dark:text-white">
            {title}
          </h1>
          {description && (
            <p className="text-sm text-slate-600 dark:text-slate-400">
              {description}
            </p>
          )}
        </div>

        <div className="flex items-center gap-3">
          {wsConnected !== undefined && (
            <Badge variant={wsConnected ? "default" : "destructive"} className="gap-1">
              <span className={`h-2 w-2 rounded-full ${wsConnected ? 'bg-green-400 animate-pulse' : 'bg-red-400'}`} />
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
    </header>
  );
}
