pq2r(P,Q)=
{
    return(subst(subst(P,x,(z+w)/2),y,(z+w)/2/I)+I*subst(subst(Q,x,(z+w)/2),y,(z+w)/2/I));
}


pol2vec(P,n,vx,vy)=
{
    my(aux);
    aux = vector(n+1);
    for(i=1,n,
        aux[i]=polcoeff(P,n-i+1,vx);
        if(i>1,
            aux[i]=polcoeff(aux[i],i,vy);
        );
    );
    return(aux);
}

vec2pol(v)=
{
    my(pol,n);
    n=#v;
    for(i=1,n,
        pol += v[i]*z^i*w^(n-i);
    );
    return(pol)
}


/* multiplies 2 homogeneous polynomials as vectors */
vpolmult(P,Q)=
{
    my(len,res,aux);
    len = #P + #Q - 1;
    res = vector(len);

    for(j=1,#P,
        aux = vector(len);
        for (i=1,#Q,
            aux[i+j-1] = P[j]*Q[i];
        );
        res += aux;
    );
    return(res)
};

/* differentiates a homogeneous polynomial with resp. to z */
vpoldz(P)=
{
    my(deg,res);
    deg = #P-1;
    res = vector(deg);

    for(i=1,deg,
        res[i] = (deg-i+1)*P[i];
       );
    return(res)
};

/* differentiates a homogeneous polynomial with resp. to w */
vpoldw(P)=
{
    my(deg,res);
    deg = #P-1;
    res = vector(deg);

    for(i=1,deg,
        res[deg-i+1] = (deg-i+1)*P[#P-i+1];
       );
    return(res)
};

/* Create diagonal matrix of coefficients */
diagmat(ord)=
{
    my(aux);
    aux = vector(ord+1);
    for(i=1,ord+1,aux[i]=ord-2*(i-1););
    return(aux)
};

/* H and R are lists with the vectors needed to compute up to desired
 * order ord (assume #H = #R)
 */
indcoef(deg,H,R)=
{
    my(res,aux);
    res = vector(deg+1);

    for(i=1,#H,
        for(j=1,#R,
            if(#H[i]+#R[j]-3==deg,
                /* Inverse order of conjugate vector */
                aux = vector(#R[j]);
                for(k=1,#aux,
                    aux[k] = conj(R[j][#R[j]-k+1]);
                );
                res += vpolmult(vpoldz(H[i]),R[j])+vpolmult(vpoldw(H[i]),aux);
            );
        );
    );
    return(I*res)
};


/* Calcula les N primeres constants de Lyapunov del sistema donat */
lyapunov(N,R)=
{
    my(lastdg,H,L);
    lastdg = N*N+3*N-7;
    H=List([[0,1,0]]);
    L=List();
    forstep(i=3,lastdg-1,2,
        /* Part senar */
        g=indcoef(i,H,R);
        d=diagmat(i);
        h=vector(i+1);
        for(j=1,i+1,
            h[j]=g[j]/d[j];
        );
        listput(H,h);
        /* Part parella */
        g=indcoef(i+1,H,R);
        d=diagmat(i+1);
        h=vector(i+2);
        listput(L,g[((i+1)/2)+1]/I);
        for(j=1,i+2,
            if(d[j]!=0,
                h[j]=g[j]/d[j];
            );
        );
        h[((i+1)/2)+1]=g[((i+1)/2)+1]/I;
        listput(H,h);
    );
    return(L);
};

/* Calcula la primera constant de Lyapunov no nul·la i la retorna,
   nomes busca fins la constant N
 */
firstlyapunov(R)=
{
    firstlyapunovN(1,R);
};

firstlyapunovN(NN,R)=
{
    my(lastdg,H,L,N,g,d,h,k,l);
    N=0;
    k=0;
    l=0;
    for(i=1,#R,
        N = max(N,#R[i]);
    );
    maxL = N*N+3*N-7;
    lastdg = 2*(maxL+1);
    H=List([[0,1,0]]);
    L=List();
    forstep(i=3,lastdg-1,2,
        /* Part senar */
        g=indcoef(i,H,R);
        d=diagmat(i);
        h=vector(i+1);
        for (j=1,i+1,
            h[j]=g[j]/d[j];
        );
        listput(H,h);
        /* Part parella */
        k++;
        g=indcoef(i+1,H,R);
        if(g[((i+1)/2)+1]!=0,
            l++;
            listput(L,[k,g[((i+1)/2)+1]/I]);
        );
        if(l==NN,
            return(L);
        );
        d=diagmat(i+1);
        h=vector(i+2);
        for(j=1,i+2,
            if(d[j]!=0,
                h[j]=g[j]/d[j];
            );
        );
        listput(H,h);
    );
    return("Centre?");
}

nextlyapunov(R,H=List([[0,1,0]]),L=List())=
{
    my(i,N,maxL,lastdg,g,d,h);
    if (#H%2==0,
        return("Invalid H");
    );
    i = #H+2;
    k = floor(i/2)-1;
    N = 0;
    for(i=1,#R,
        N = max(N,#R[i]);
    );
    maxL = N*N+3*N-7;
    lastdg = 2*(maxL+1);
    while (i<lastdg,
        /* Odd degree */
        g = indcoef(i,H,R);
        d = diagmat(i);
        h = vector(i+1);
        for (j=1,i+1,
            h[j] = g[j]/d[j];
        );
        listput(H,h);
        /* Even degree */
        k++;
        g = indcoef(i+1,H,R);
        d = diagmat(i+1);
        h = vector(i+2);
        for (j=1,i+2,
            if (d[j]!=0,
                h[j] = g[j]/d[j];
            );
        );
        listput(H,h);
        const = g[((i+1)/2)+1]/I;
        if (const!=0,
            listput(L,[k,const]);
            /* Return H and the constant found */
            return(List([L,H]));
        );
        i+=2;
    );       
    return(-1);
}
