'use client';

import { Card, CardContent } from '@/components/ui/card';
import { FileText } from 'lucide-react';
import { Layout, PageHeader } from '@/components/layout';

export default function ReportsPage() {
  return (
    <Layout>
      <PageHeader title="Reports" description="Generate and view reports" />
      <main className="container mx-auto px-6 py-8">
        <Card>
          <CardContent className="flex flex-col items-center justify-center py-16">
            <FileText className="h-16 w-16 text-muted-foreground mb-4" />
            <h3 className="text-xl font-semibold mb-2">Reports Coming Soon</h3>
            <p className="text-sm text-muted-foreground">
              Export motion events, camera logs, and system reports
            </p>
          </CardContent>
        </Card>
      </main>
    </Layout>
  );
}
